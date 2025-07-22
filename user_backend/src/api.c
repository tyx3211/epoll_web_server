#define _DEFAULT_SOURCE // For strdup
#include "api.h"
#include "server.h" // For queue_data_for_writing
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h" // for get_query_param
#include <stdbool.h> // For bool
#include <l8w8jwt/encode.h>
#include <time.h>
#include "auth.h" // Include our new auth module

// IMPORTANT: This should be loaded from a secure configuration, not hardcoded!
// const char JWT_SECRET[] = "a-very-secret-and-long-key-that-is-at-least-32-bytes";

void handle_api_login(Connection* conn, ServerConfig* config, int epollFd) {
    char* username = NULL;
    char* password = NULL;
    const char* response_body;
    bool credentials_valid = false; // Moved declaration to top of function scope

    if (conn->request.body) {
        // Login still uses POST body
        username = get_query_param(conn->request.body, "username");
        password = get_query_param(conn->request.body, "password");
    }

    // printf("username: %s, password: %s\n", username, password);

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
                    // Very simple CSV parsing: expects "user,pass\n"
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
                response_body = strdup(json_response);
                free(token_str);
            } else {
                response_body = "{\"status\":\"error\", \"message\":\"Internal server error: could not create token.\"}";
            }
        } else {
            log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 401);
            response_body = "{\"status\":\"error\", \"message\":\"Invalid credentials.\"}";
        }
    } else {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 400);
        response_body = "{\"status\":\"error\", \"message\":\"Missing username or password.\"}";
    }

    char header[512];
    int headerLen = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Connection: close\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %ld\r\n\r\n",
                             strlen(response_body));
    
    queue_data_for_writing(conn, header, headerLen, epollFd);
    queue_data_for_writing(conn, response_body, strlen(response_body), epollFd);

    // Free the response_body ONLY if it was dynamically allocated by strdup
    if (credentials_valid) {
        free((void*)response_body);
    }

    free(username);
    free(password);
}

void handle_api_register(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config; // Unused
    char* username = NULL;
    char* password = NULL;
    const char* response_body;
    int status_code = 500; // Default to internal server error

    if (conn->request.body) {
        username = get_query_param(conn->request.body, "username");
        password = get_query_param(conn->request.body, "password");
    }

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
            status_code = 409; // Conflict
            response_body = "{\"status\":\"error\", \"message\":\"Username already exists.\"}";
        } else {
            // --- Append new user to CSV ---
            fp = fopen("www/data/users.csv", "a");
            if (fp) {
                fprintf(fp, "\n%s,%s", username, password);
                fclose(fp);
                status_code = 201; // Created
                response_body = "{\"status\":\"success\", \"message\":\"User registered successfully.\"}";
            } else {
                log_system(LOG_ERROR, "Could not open users.csv for appending.");
                status_code = 500;
                response_body = "{\"status\":\"error\", \"message\":\"Internal server error.\"}";
            }
        }
    } else {
        status_code = 400; // Bad Request
        response_body = "{\"status\":\"error\", \"message\":\"Missing username or password.\"}";
    }

    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, status_code);

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Connection: close\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %ld\r\n\r\n",
             status_code, 
             status_code == 201 ? "Created" : (status_code == 409 ? "Conflict" : (status_code == 400 ? "Bad Request" : "Internal Server Error")),
             strlen(response_body));
    
    queue_data_for_writing(conn, header, strlen(header), epollFd);
    queue_data_for_writing(conn, response_body, strlen(response_body), epollFd);

    free(username);
    free(password);
}


void handle_api_upload_test(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config; // config is unused in this handler
    log_system(LOG_INFO, "Upload Test API: Received request with Content-Length: %zu", conn->request.content_length);
    
    char response_body[256];
    
    // The size of large_post_body.txt is 5 * 1024 * 1024 = 5242880 bytes.
    if (conn->request.content_length == 5242880) {
        log_system(LOG_INFO, "Upload Test API: Successfully received the complete large request body (%zu bytes).", conn->request.content_length);
        snprintf(response_body, sizeof(response_body), "{\"status\":\"success\", \"message\":\"Received %zu bytes.\"}", conn->request.content_length);
    } else {
        log_system(LOG_WARNING, "Upload Test API: Received incomplete body. Expected %d, got %zu.", 5242880, conn->request.content_length);
        snprintf(response_body, sizeof(response_body), "{\"status\":\"error\", \"message\":\"Expected 5242880 bytes, but received %zu.\"}", conn->request.content_length);
    }

    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);

    char header[512];
    int headerLen = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Connection: close\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: %ld\r\n\r\n",
                             strlen(response_body));
    
    queue_data_for_writing(conn, header, headerLen, epollFd);
    queue_data_for_writing(conn, response_body, strlen(response_body), epollFd);
}

// Helper function to process search logic, used by both GET and POST handlers
static void process_search_and_respond(Connection* conn, int epollFd, const char* filename_key, const char* search_key) {
    char response_buffer[4096] = {0}; // Buffer for the search results
    char error_msg[256] = {0};

    if (filename_key && search_key) {
        char filepath[512];
        // Now reads from .csv files
        snprintf(filepath, sizeof(filepath), "www/data/%s.csv", filename_key);
        
        if (strstr(filepath, "..") != NULL) {
             snprintf(error_msg, sizeof(error_msg), "Invalid filename.");
        } else {
            FILE* fp = fopen(filepath, "r");
            if (fp) {
                char line[1024];
                // Skip header line
                if (fgets(line, sizeof(line), fp)) {
                    // Build a CSV response of matching lines
                    while(fgets(line, sizeof(line), fp)) {
                        if (strstr(line, search_key)) {
                            // Append each matching line to the buffer.
                            // Lines are already newline-terminated.
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
    
    const char* body = strlen(error_msg) > 0 ? error_msg : (strlen(response_buffer) > 0 ? response_buffer : "No results found.");
    
    log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);

    char header[512];
    int headerLen = snprintf(header, sizeof(header),
                             "HTTP/1.1 200 OK\r\n"
                             "Connection: close\r\n"
                             "Content-Type: text/plain; charset=utf-8\r\n"
                             "Content-Length: %ld\r\n\r\n",
                             strlen(body));

    queue_data_for_writing(conn, header, headerLen, epollFd);
    queue_data_for_writing(conn, body, strlen(body), epollFd);
}

void handle_api_search(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config;  // Mark as unused
    
    char* filename_key = NULL;
    char* search_key = NULL;

    // GET request: parse from query string
    if (conn->request.raw_query_string) {
        filename_key = get_query_param(conn->request.raw_query_string, "filename");
        search_key = get_query_param(conn->request.raw_query_string, "keyword");
    }

    process_search_and_respond(conn, epollFd, filename_key, search_key);

    free(filename_key);
    free(search_key);
} 

void handle_api_search_post(Connection* conn, ServerConfig* config, int epollFd) {
    (void)config; // Mark as unused

    char* filename_key = NULL;
    char* search_key = NULL;

    // POST request: parse from request body
    if (conn->request.body) {
        filename_key = get_query_param(conn->request.body, "filename");
        search_key = get_query_param(conn->request.body, "keyword");
    }

    process_search_and_respond(conn, epollFd, filename_key, search_key);

    free(filename_key);
    free(search_key);
}

void handle_api_me(Connection* conn, ServerConfig* config, int epollFd) {
    char* authed_user = authenticate_request(conn, config);

    if (authed_user) {
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 200);
        char response_body[256];
        snprintf(response_body, sizeof(response_body), "{\"status\":\"success\", \"user\":{\"username\":\"%s\"}}", authed_user);

        char header[512];
        int headerLen = snprintf(header, sizeof(header),
                                 "HTTP/1.1 200 OK\r\n"
                                 "Connection: close\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: %ld\r\n\r\n",
                                 strlen(response_body));
        
        queue_data_for_writing(conn, header, headerLen, epollFd);
        queue_data_for_writing(conn, response_body, strlen(response_body), epollFd);
        
        free(authed_user); // Free the username string returned by the auth function
    } else {
        // Authentication failed - This part is already correct
        log_access(conn->client_ip, conn->request.method, conn->request.raw_uri, 401);
        char response[] = "HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\nUnauthorized";
        queue_data_for_writing(conn, response, sizeof(response) - 1, epollFd);
    }
} 