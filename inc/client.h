#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>

typedef enum {
    ORKA_HTTP_INVALID,
    ORKA_HTTP_1_0,
    ORKA_HTTP_1_1,
}
orka_http_version_t;

typedef struct {
    pthread_t thread;
    struct lua_State* lua; // starts with handler function on stack
    int fd;

    orka_http_version_t http_version;
    bool keep_alive;
    bool chunked;
}
orka_client_t;

typedef struct {
    orka_client_t* client;
    const char* abort_status;
    http_parser parser;
    int header_table_ref;
    int response_ref;

    char url[1024];
    char header_field[1024];
    char header_value[1024];
}
orka_request_t;

void
orka_start_client(orka_client_t* client);

#endif
