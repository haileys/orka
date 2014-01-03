#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

typedef struct {
    struct lua_State* lua;
    pthread_t thread;
    uint16_t port;
    int fd;
    int handler_ref;
}
orka_server_t;

#endif
