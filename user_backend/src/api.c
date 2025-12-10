#define _DEFAULT_SOURCE // For strdup
#include "api.h"
#include "server.h"   // For Connection struct
#include "response.h" // Phase 1: Response helper API
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>     // For time(), localtime(), strftime()
#include "utils.h"    // Phase 2: http_get_param() - no more manual free!
#include <stdbool.h>
#include "auth.h"     // For generate_token_for_user, authenticate_request
#include "yyjson.h"   // Phase 3: JSON support

// ============================================================================
// API Handler: POST /api/login
// ============================================================================
void handle_api_login(Connection* conn, ServerConfig* config, int epollFd) {
    bool credentials_valid = false;

    // Phase 2: Use pre-parsed body params (no need to free!)
    const char* username = http_get_body_param(&conn->request, "username");
    const char* password = http_get_body_param(&conn->request, "password");

    if (username && password) {
        log_system(LOG_INFO, "Login attempt: user=%s", username);

        // --- CSV-based user validation ---
        FILE* fp = fopen("www/data/users.csv", "r");
        if (fp) {
            char line[256];
            char file_user[128], file_pass[128];
            
            // Skip header
            if (fgets(line, sizeof(line), fp)) {
                while (fgets(line, sizeof(line), fp)) {
                    if (sscanf(line, "%127[^,],%127s", file_user, file_pass) == 2) {
                        if (strcmp(file_user, username) == 0 && strcmp(file_pass, password) == 0) {
                            credentials_valid = true;
                            break;
                        }
                    }
                }
            }
            fclose(fp);
        } else {
            log_system(LOG_ERROR, "Could not open users.csv");
        }
        
        if (credentials_valid) {
            log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
            char* token_str = generate_token_for_user(username, config);
            if (token_str) {
                char json_response[1024];
                snprintf(json_response, sizeof(json_response),
                         "{\"status\":\"success\", \"token\":\"%s\"}", token_str);
                http_send_json(conn, 200, json_response, epollFd);
                free(token_str);
            } else {
                http_send_json(conn, 500, "{\"status\":\"error\", \"message\":\"Internal server error: could not create token.\"}", epollFd);
            }
        } else {
            log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 401);
            http_send_json(conn, 401, "{\"status\":\"error\", \"message\":\"Invalid credentials.\"}", epollFd);
        }
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\", \"message\":\"Missing username or password.\"}", epollFd);
    }
    // No need to free username/password - they're pointers to pre-parsed data!
}

// ============================================================================
// API Handler: POST /api/register
// ============================================================================
void handle_api_register(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config; // Unused
    int status_code = 500;
    const char* response_body = "{\"status\":\"error\", \"message\":\"Internal server error.\"}";

    // Phase 2: Use pre-parsed body params
    const char* username = http_get_body_param(&conn->request, "username");
    const char* password = http_get_body_param(&conn->request, "password");

    if (username && password) {
        // --- Check if user already exists ---
        bool user_exists = false;
        FILE* fp = fopen("www/data/users.csv", "r");
        if (fp) {
            char line[256];
            char file_user[128];
            while (fgets(line, sizeof(line), fp)) {
                if (sscanf(line, "%127[^,],", file_user) == 1) {
                    if (strcmp(file_user, username) == 0) {
                        user_exists = true;
                        break;
                    }
                }
            }
            fclose(fp);
        }

        if (user_exists) {
            status_code = 409;
            response_body = "{\"status\":\"error\", \"message\":\"Username already exists.\"}";
        } else {
            // --- Append new user to CSV ---
            fp = fopen("www/data/users.csv", "a");
            if (fp) {
                fprintf(fp, "\n%s,%s", username, password);
                fclose(fp);
                status_code = 201;
                response_body = "{\"status\":\"success\", \"message\":\"User registered successfully.\"}";
            } else {
                log_system(LOG_ERROR, "Could not open users.csv for appending.");
                status_code = 500;
                response_body = "{\"status\":\"error\", \"message\":\"Internal server error.\"}";
            }
        }
    } else {
        status_code = 400;
        response_body = "{\"status\":\"error\", \"message\":\"Missing username or password.\"}";
    }

    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, status_code);
    http_send_json(conn, status_code, response_body, epollFd);
    // No need to free!
}

// ============================================================================
// API Handler: POST /api/upload_test
// ============================================================================
void handle_api_upload_test(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;
    log_system(LOG_INFO, "Upload Test API: Received request with Content-Length: %zu", conn->request.content_length);
    
    char response_body[256];
    int status_code = 200;
    
    // The size of large_post_body.txt is 5 * 1024 * 1024 = 5242880 bytes.
    if (conn->request.content_length == 5242880) {
        log_system(LOG_INFO, "Upload Test API: Successfully received the complete large request body (%zu bytes).", conn->request.content_length);
        snprintf(response_body, sizeof(response_body), 
                 "{\"status\":\"success\", \"message\":\"Received %zu bytes.\"}", conn->request.content_length);
    } else {
        log_system(LOG_WARNING, "Upload Test API: Received incomplete body. Expected %d, got %zu.", 5242880, conn->request.content_length);
        snprintf(response_body, sizeof(response_body), 
                 "{\"status\":\"error\", \"message\":\"Expected 5242880 bytes, but received %zu.\"}", conn->request.content_length);
    }

    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, status_code);
    http_send_json(conn, status_code, response_body, epollFd);
}

// ============================================================================
// Helper: Process search logic (used by both GET and POST handlers)
// ============================================================================
static void process_search_and_respond(Connection* conn, int epollFd, const char* filename_key, const char* search_key) {
    char response_buffer[4096] = {0};
    char error_msg[256] = {0};

    if (filename_key && search_key) {
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "www/data/%s.csv", filename_key);
        
        if (strstr(filepath, "..") != NULL) {
             snprintf(error_msg, sizeof(error_msg), "Invalid filename.");
        } else {
            FILE* fp = fopen(filepath, "r");
            if (fp) {
                char line[1024];
                // Skip header line
                if (fgets(line, sizeof(line), fp)) {
                    while(fgets(line, sizeof(line), fp)) {
                        if (strstr(line, search_key)) {
                            strncat(response_buffer, line, sizeof(response_buffer) - strlen(response_buffer) - 1);
                        }
                    }
                }
                fclose(fp);
            } else {
                 snprintf(error_msg, sizeof(error_msg), "File not found: %s.csv", filename_key);
            }
        }
    } else {
        snprintf(error_msg, sizeof(error_msg), "Missing filename or keyword.");
    }
    
    const char* body = strlen(error_msg) > 0 ? error_msg : 
                       (strlen(response_buffer) > 0 ? response_buffer : "No results found.");
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
    http_send_text(conn, 200, body, epollFd);
}

// ============================================================================
// API Handler: GET /api/search
// ============================================================================
void handle_api_search(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;
    
    // Phase 2: Use pre-parsed query params (no need to free!)
    const char* filename_key = http_get_query_param(&conn->request, "filename");
    const char* search_key = http_get_query_param(&conn->request, "keyword");

    process_search_and_respond(conn, epollFd, filename_key, search_key);
    // No need to free!
} 

// ============================================================================
// API Handler: POST /api/search
// ============================================================================
void handle_api_search_post(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;

    // Phase 2: Use pre-parsed body params (no need to free!)
    const char* filename_key = http_get_body_param(&conn->request, "filename");
    const char* search_key = http_get_body_param(&conn->request, "keyword");

    process_search_and_respond(conn, epollFd, filename_key, search_key);
    // No need to free!
}

// ============================================================================
// API Handler: GET /api/me (Protected - requires valid JWT)
// ============================================================================
void handle_api_me(Connection* conn, ServerConfig* config, int epollFd) {
    char* authed_user = authenticate_request(conn, config);

    if (authed_user) {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
        
        char response_body[256];
        snprintf(response_body, sizeof(response_body), 
                 "{\"status\":\"success\", \"user\":{\"username\":\"%s\"}}", authed_user);
        
        http_send_json(conn, 200, response_body, epollFd);
        free(authed_user);
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 401);
        http_send_error(conn, 401, "Unauthorized", epollFd);
    }
}

// ============================================================================
// Phase 3 Demo: POST /api/json_echo
// Accepts JSON body, reads fields, and responds with a yyjson-built JSON
// ============================================================================
void handle_api_json_echo(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;
    
    // Check if JSON body was parsed
    if (!conn->request.json_doc || !conn->request.json_root) {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\", \"message\":\"Expected JSON body\"}", epollFd);
        return;
    }
    
    // Read fields from request JSON (immutable document)
    yyjson_val* root = conn->request.json_root;
    yyjson_val* name_val = yyjson_obj_get(root, "name");
    yyjson_val* age_val = yyjson_obj_get(root, "age");
    
    const char* name = name_val ? yyjson_get_str(name_val) : "unknown";
    int age = age_val ? (int)yyjson_get_int(age_val) : 0;
    
    log_system(LOG_INFO, "JSON Echo: Received name=%s, age=%d", name, age);
    
    // Build response JSON using yyjson mutable document
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* res_root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, res_root);
    
    yyjson_mut_obj_add_str(doc, res_root, "status", "success");
    yyjson_mut_obj_add_str(doc, res_root, "message", "JSON echo successful");
    
    // Add echo object
    yyjson_mut_val* echo_obj = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_str(doc, echo_obj, "received_name", name);
    yyjson_mut_obj_add_int(doc, echo_obj, "received_age", age);
    yyjson_mut_obj_add_int(doc, echo_obj, "age_plus_ten", age + 10);
    yyjson_mut_obj_add_val(doc, res_root, "echo", echo_obj);
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
    
    // Send response using our new Phase 3 API
    http_send_json_doc(conn, 200, doc, epollFd);
    
    // Clean up
    yyjson_mut_doc_free(doc);
}

// ============================================================================
// Demo API: GET /api/system_info
// Returns server information using yyjson
// ============================================================================
void handle_api_system_info(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;

    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_obj_add_str(doc, root, "name", "Epoll Server Core");
    yyjson_mut_obj_add_str(doc, root, "version", "1.0.0");
    yyjson_mut_obj_add_str(doc, root, "phase", "3");
    yyjson_mut_obj_add_str(doc, root, "uptime", "running");
    
    // Features array
    yyjson_mut_val* features = yyjson_mut_arr(doc);
    yyjson_mut_arr_add_str(doc, features, "Epoll Reactor Model");
    yyjson_mut_arr_add_str(doc, features, "HTTP State Machine Parser");
    yyjson_mut_arr_add_str(doc, features, "Response API");
    yyjson_mut_arr_add_str(doc, features, "Param Pre-parsing");
    yyjson_mut_arr_add_str(doc, features, "JSON Support (yyjson)");
    yyjson_mut_arr_add_str(doc, features, "JWT Authentication");
    yyjson_mut_obj_add_val(doc, root, "features", features);
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
    http_send_json_doc(conn, 200, doc, epollFd);
    yyjson_mut_doc_free(doc);
}

// ============================================================================
// Demo API: POST /api/calc
// Simple calculator using JSON input/output
// ============================================================================
void handle_api_calc(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;
    
    if (!conn->request.json_doc || !conn->request.json_root) {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Expected JSON body\"}", epollFd);
        return;
    }
    
    yyjson_val* root = conn->request.json_root;
    yyjson_val* a_val = yyjson_obj_get(root, "a");
    yyjson_val* b_val = yyjson_obj_get(root, "b");
    yyjson_val* op_val = yyjson_obj_get(root, "op");
    
    if (!a_val || !b_val || !op_val) {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Missing a, b, or op\"}", epollFd);
        return;
    }
    
    // Handle both integer and real numbers
    double a, b;
    if (yyjson_is_int(a_val)) {
        a = (double)yyjson_get_int(a_val);
    } else if (yyjson_is_real(a_val)) {
        a = yyjson_get_real(a_val);
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Invalid type for 'a'\"}", epollFd);
        return;
    }
    
    if (yyjson_is_int(b_val)) {
        b = (double)yyjson_get_int(b_val);
    } else if (yyjson_is_real(b_val)) {
        b = yyjson_get_real(b_val);
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Invalid type for 'b'\"}", epollFd);
        return;
    }
    
    const char* op = yyjson_get_str(op_val);
    
    double result = 0;
    const char* op_symbol = "+";
    
    if (strcmp(op, "add") == 0) {
        result = a + b;
        op_symbol = "+";
    } else if (strcmp(op, "sub") == 0) {
        result = a - b;
        op_symbol = "-";
    } else if (strcmp(op, "mul") == 0) {
        result = a * b;
        op_symbol = "×";
    } else if (strcmp(op, "div") == 0) {
        if (b == 0) {
            log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
            http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Division by zero\"}", epollFd);
            return;
        }
        result = a / b;
        op_symbol = "÷";
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        http_send_json(conn, 400, "{\"status\":\"error\",\"message\":\"Unknown operation\"}", epollFd);
        return;
    }
    
    // Build response
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* res_root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, res_root);
    
    yyjson_mut_obj_add_str(doc, res_root, "status", "success");
    yyjson_mut_obj_add_real(doc, res_root, "a", a);
    yyjson_mut_obj_add_str(doc, res_root, "op", op_symbol);
    yyjson_mut_obj_add_real(doc, res_root, "b", b);
    yyjson_mut_obj_add_real(doc, res_root, "result", result);
    
    char expr[64];
    snprintf(expr, sizeof(expr), "%.2f %s %.2f = %.2f", a, op_symbol, b, result);
    yyjson_mut_obj_add_str(doc, res_root, "expression", expr);
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
    http_send_json_doc(conn, 200, doc, epollFd);
    yyjson_mut_doc_free(doc);
}

// ============================================================================
// Demo API: GET /api/time
// Returns current server time
// ============================================================================
void handle_api_time(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char date_str[32];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", tm_info);
    
    char clock_str[16];
    strftime(clock_str, sizeof(clock_str), "%H:%M:%S", tm_info);
    
    yyjson_mut_doc* doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    
    yyjson_mut_obj_add_str(doc, root, "status", "success");
    yyjson_mut_obj_add_str(doc, root, "datetime", time_str);
    yyjson_mut_obj_add_str(doc, root, "date", date_str);
    yyjson_mut_obj_add_str(doc, root, "time", clock_str);
    yyjson_mut_obj_add_int(doc, root, "timestamp", (int64_t)now);
    yyjson_mut_obj_add_str(doc, root, "timezone", "Local");
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
    http_send_json_doc(conn, 200, doc, epollFd);
    yyjson_mut_doc_free(doc);
}
