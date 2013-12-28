#include <http_parser.h>
#include <stdlib.h>

#include "orka.h"

static void
on_close(uv_handle_t* handle)
{
    orka_server_t* server = handle->data;
    luaL_unref(server->lua, LUA_REGISTRYINDEX, server->handler_ref);
    free(server);
}

static void
on_connection(uv_stream_t* server_, int status)
{
    if(status) {
        fprintf(stderr, "on_connection: %s\n", uv_strerror(uv_last_error(orka_loop)));
        return;
    }

    orka_server_t* server = server_->data;
    orka_client_t* client = malloc(sizeof(*client));

    client->lua = server->lua;
    client->handler_ref = server->handler_ref;

    client->header_table_ref = LUA_NOREF;

    if(uv_tcp_init(orka_loop, &client->client)) {
        fprintf(stderr, "on_connection: %s\n", uv_strerror(uv_last_error(orka_loop)));
        free(client);
        return;
    }

    // guaranteed to succeed
    uv_accept((uv_stream_t*)&server->server, (uv_stream_t*)&client->client);

    client->client.data = client;

    http_parser_init(&client->parser, HTTP_REQUEST);
    client->parser.data = client;

    orka_start_client(client);
}

static int
serve(lua_State* l)
{
    int port = luaL_checkint(l, 1);
    luaL_checktype(l, 2, LUA_TFUNCTION);

    if(port <= 0 || port > 65535) {
        return luaL_error(l, "port number must be within between 1 and 65535");
    }

    orka_server_t* server = malloc(sizeof(*server));
    server->port = port;
    server->lua = l;

    lua_pushvalue(l, 2);
    server->handler_ref = luaL_ref(l, LUA_REGISTRYINDEX);

    if(uv_tcp_init(orka_loop, &server->server)) {
        free(server);
        return orka_uv_error(l, "uv_tcp_init");
    }

    server->server.data = server;

    if(uv_tcp_bind(&server->server, uv_ip4_addr("0.0.0.0", server->port))) {
        free(server);
        return orka_uv_error(l, "uv_tcp_bind");
    }

    if(uv_listen((uv_stream_t*)&server->server, 16, on_connection)) {
        uv_close((uv_handle_t*)&server->server, on_close);
        return orka_uv_error(l, "uv_listen");
    }

    return 0;
}

void
orka_init_http_lib()
{
    lua_pushcfunction(orka_lua, serve);
    lua_setglobal(orka_lua, "serve");
}
