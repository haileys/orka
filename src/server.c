#include <arpa/inet.h>
#include <errno.h>
#include <http_parser.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orka.h"
#include "client.h"
#include "server.h"

static void*
client_thread_main(void* ctx)
{
    orka_client_t* client = ctx;
    pthread_detach(client->thread);
    orka_start_client(client);
    return NULL;
}

static void*
server_thread_main(void* ctx)
{
    orka_server_t* server = ctx;
    pthread_detach(server->thread);

    while(1) {
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server->fd, &client_addr, &client_addr_len);

        if(client_fd < 0) {
            if(errno == EINTR) {
                continue;
            }
            orka_error(server->lua, "accept");
        }

        orka_client_t* client = malloc(sizeof(*client));
        orka_gil_acquire();
        client->lua = lua_newthread(server->lua);
        lua_rawgeti(server->lua, LUA_REGISTRYINDEX, server->handler_ref);
        lua_xmove(server->lua, client->lua, 1);
        orka_gil_release();
        client->fd = client_fd;
        pthread_create(&client->thread, NULL, client_thread_main, client);
    }

    return NULL;
}

static int
setup_listener(const char* ipv4, uint16_t port)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd < 0) {
        return -1;
    }

    int one = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(fd);
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if(!inet_aton(ipv4, &addr.sin_addr)) {
        close(fd);
        return -1;
    }

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if(listen(fd, 16) < 0) {
        close(fd);
        return -1;
    }

    return fd;
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

    server->fd = setup_listener("0.0.0.0", server->port);
    if(server->fd < 0) {
        free(server);
        return orka_error(l, "setup_listener");
    }

    if(pthread_create(&server->thread, NULL, server_thread_main, server) < 0) {
        return orka_error(l, "pthread_create");
    }

    return 0;
}

void
orka_init_http_lib()
{
    lua_pushcfunction(orka_lua, serve);
    lua_setglobal(orka_lua, "serve");
}
