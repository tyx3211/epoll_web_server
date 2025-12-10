#ifndef API_H
#define API_H

#include "http.h" // Includes Connection and ServerConfig

/**
 * @brief Handles the /api/login POST request.
 * A simple example to show how to handle form data.
 */
void handle_api_login(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Handles the /api/register POST request.
 * Creates a new user if the username is not taken.
 */
void handle_api_register(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Handles the /api/upload_test POST request.
 * A test endpoint to verify handling of large request bodies.
 */
void handle_api_upload_test(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Handles the /api/search POST request.
 * Implements the search logic described in the requirements.
 */
void handle_api_search(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Handles the /api/search POST request (alternative endpoint).
 * Implements the same search logic but via POST method.
 */
void handle_api_search_post(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Handles the /api/me GET request.
 * Returns information about the authenticated user.
 */
void handle_api_me(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Phase 3 Demo: Handles the /api/json_echo POST request.
 * Accepts JSON body, reads fields using yyjson, and responds with yyjson-built JSON.
 */
void handle_api_json_echo(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Demo API: GET /api/system_info
 * Returns server information using yyjson.
 */
void handle_api_system_info(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Demo API: POST /api/calc
 * Simple calculator using JSON input/output.
 */
void handle_api_calc(Connection* conn, ServerConfig* config, int epollFd);

/**
 * @brief Demo API: GET /api/time
 * Returns current server time.
 */
void handle_api_time(Connection* conn, ServerConfig* config, int epollFd);

#endif // API_H