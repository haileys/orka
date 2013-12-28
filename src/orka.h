#ifndef ORKA_H
#define ORKA_H

#include <http_parser.h>
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <stdbool.h>
#include <uv.h>

extern lua_State*
orka_lua;

extern uv_loop_t*
orka_loop;

typedef struct {
    lua_State* lua;
    int port;
    uv_tcp_t server;
    int handler_ref;
}
orka_server_t;

typedef struct {
    lua_State* lua;
    uv_tcp_t client;
    http_parser parser;
    int handler_ref; /* weak */

    const char* abort_status;

    char url[1024];
    int header_table_ref;
    char header_field[1024];
    char header_value[1024];

    // this buffer is used to fulfill libuv's alloc requests
    bool scratch_used;
    char scratch[1024];
}
orka_client_t;

int
orka_uv_error(lua_State* l, const char* function_name);

void
orka_start_client(orka_client_t* client);

#endif
