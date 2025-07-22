#include "server.h"
#include <stdio.h>
#include "router.h"
#include "api.h"

int main(int argc, char* argv[]) {
    const char* config_path = NULL;
    if (argc > 1) {
        config_path = argv[1];
        printf("Using configuration file: %s\n", config_path);
    } else {
        printf("No configuration file specified, using default settings.\n");
    }

    // Initialize the router and register routes
    router_init();
    router_add_route("POST", "/api/login", handle_api_login);
    router_add_route("POST", "/api/register", handle_api_register);
    router_add_route("GET", "/api/search", handle_api_search); // Changed from POST to GET
    router_add_route("POST", "/api/search", handle_api_search_post);
    router_add_route("GET", "/api/me", handle_api_me);
    router_add_route("POST", "/api/upload_test", handle_api_upload_test);
    printf("Registered API routes.\n");

    startServer(config_path);
    return 0;
} 