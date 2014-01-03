#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "orka.h"
#include "buffer.h"
#include "client.h"

static int
on_message_begin(http_parser* parser)
{
    (void)parser;
    return 0;
}

static int
static_append(char* buff, size_t cap, const char* str, size_t len)
{
    size_t existing_len = strlen(buff);
    if(existing_len + len + 1 >= cap) {
        return 1;
    }

    memcpy(buff + existing_len, str, len);
    buff[existing_len + len] = 0;

    return 0;
}

static int
on_url(http_parser* parser, const char* ptr, size_t len)
{
    orka_request_t* request = parser->data;

    if(static_append(request->url, sizeof(request->url), ptr, len)) {
        request->abort_status = "414 Request-URI Too Long";
        return -1;
    }

    return 0;
}

static void
save_header_field_value(orka_request_t* request)
{
    for(char* ptr = request->header_field; *ptr; ptr++) {
        char c = *ptr;

        if(c >= 'A' && c <= 'Z') {
            *ptr = c - 'A' + 'a';
        } else if(c == '-') {
            *ptr = '_';
        }
    }

    orka_gil_acquire();
    lua_rawgeti(request->client->lua, LUA_REGISTRYINDEX, request->header_table_ref);
    lua_pushstring(request->client->lua, request->header_value);
    lua_setfield(request->client->lua, -2, request->header_field);
    lua_pop(request->client->lua, 1);
    orka_gil_release();

    request->header_field[0] = 0;
    request->header_value[0] = 0;
}

static int
on_header_field(http_parser* parser, const char* ptr, size_t len)
{
    orka_request_t* request = parser->data;

    if(request->header_field[0]) {
        save_header_field_value(request);
    }

    if(static_append(request->header_field, sizeof(request->header_field), ptr, len)) {
        request->abort_status = "400 Bad Request (header field too long)";
        return -1;
    }

    return 0;
}

static int
on_header_value(http_parser* parser, const char* ptr, size_t len)
{
    orka_request_t* request = parser->data;

    if(static_append(request->header_value, sizeof(request->header_value), ptr, len)) {
        request->abort_status = "400 Bad Request (header value too long)";
        return -1;
    }

    return 0;
}

static int
on_headers_complete(http_parser* parser)
{
    orka_request_t* request = parser->data;

    if(request->header_field[0]) {
        save_header_field_value(request);
    }

    return -1;
}

static const http_parser_settings parser_settings = {
    .on_message_begin    = on_message_begin,
    .on_url              = on_url,
    .on_header_field     = on_header_field,
    .on_header_value     = on_header_value,
    .on_headers_complete = on_headers_complete,
};

static void
init_request(orka_request_t* request, orka_client_t* client)
{
    request->client = client;
    request->abort_status = NULL;
    http_parser_init(&request->parser, HTTP_REQUEST);
    request->parser.data = request;
    orka_gil_acquire();
    lua_newtable(client->lua);
    request->header_table_ref = luaL_ref(client->lua, LUA_REGISTRYINDEX);
    orka_gil_release();
    request->response_ref = LUA_NOREF;
    request->url[0] = 0;
    request->header_field[0] = 0;
    request->header_value[0] = 0;
}

static int
read_request(orka_client_t* client)
{
    orka_request_t request;
    init_request(&request, client);

    while(1) {
        char buff[4096];
        int nread = recv(client->fd, buff, sizeof(buff), 0);

        if(nread < 0 && errno == EINTR) {
            continue;
        }

        if(nread <= 0) {
            break;
        }

        http_parser_execute(&request.parser, &parser_settings, buff, nread);

        if(HTTP_PARSER_ERRNO(&request.parser) == HPE_OK) {
            // we haven't read the complete request headers yet, wait for more data
            continue;
        }

        if(HTTP_PARSER_ERRNO(&request.parser) == HPE_CB_headers_complete) {
            if(request.parser.http_major != 1) {
                break; // unsupported http version
            }

            if(request.parser.http_minor == 0) {
                client->http_version = ORKA_HTTP_1_0;
            } else if(request.parser.http_minor == 1) {
                client->http_version = ORKA_HTTP_1_1;
            } else {
                break; // unsupported http version
            }

            request.client->keep_alive = http_should_keep_alive(&request.parser);
            request.client->chunked = false;

            return request.header_table_ref;
        }

        // TODO - handle bad requests
        break;
    }

    orka_gil_acquire();
    luaL_unref(client->lua, LUA_REGISTRYINDEX, request.header_table_ref);
    orka_gil_release();
    return LUA_NOREF;
}

static void
fix_up_headers(orka_client_t* client)
{
    lua_getfield(client->lua, 2, "Content-Length");
    lua_getfield(client->lua, 2, "Transfer-Encoding");
    if(lua_isnoneornil(client->lua, -1) && lua_isnoneornil(client->lua, -2)) {
        lua_pushstring(client->lua, "chunked");
        lua_setfield(client->lua, 2, "Transfer-Encoding");
        client->chunked = true;
    }
    lua_pop(client->lua, 2);

    if(client->keep_alive) {
        lua_pushstring(client->lua, "close");
        lua_setfield(client->lua, 2, "Connection");
    }
}

static void
format_response_status(orka_client_t* client, orka_buffer_t* header_buff)
{
    char custom_buff[32];
    const char* status_str = NULL;

    if(lua_type(client->lua, 1) == LUA_TSTRING) {
        status_str = lua_tostring(client->lua, 1);
    } else {
        int status_code = lua_tointeger(client->lua, 1);
        if(!(status_str = orka_status_code(status_code))) {
            sprintf(custom_buff, "%d", status_code);
            status_str = custom_buff;
        }
    }

    orka_buffer_append_cstr(header_buff, status_str);
}

static void
collect_response_headers(orka_client_t* client, orka_buffer_t* buff)
{
    if(client->http_version == ORKA_HTTP_1_0) {
        orka_buffer_append_cstr(buff, "HTTP/1.0 ");
    } else {
        orka_buffer_append_cstr(buff, "HTTP/1.1 ");
    }

    format_response_status(client, buff);
    orka_buffer_append_cstr(buff, "\r\n");

    lua_pushnil(client->lua);
    while(lua_next(client->lua, 2) != 0) {
        if(lua_type(client->lua, -2) == LUA_TSTRING) {
            const char* key = lua_tostring(client->lua, -2);
            // TODO protect against response splitting:
            const char* val = lua_tostring(client->lua, -1);

            orka_buffer_append_cstr(buff, key);
            orka_buffer_append_cstr(buff, ": ");
            orka_buffer_append_cstr(buff, val);
            orka_buffer_append_cstr(buff, "\r\n");
        }
        lua_pop(client->lua, 1);
    }

    orka_buffer_append_cstr(buff, "\r\n");
}

static int
write_buffer(orka_client_t* client, orka_buffer_t* buff)
{
    size_t pos = 0;
    while(pos < buff->len) {
        ssize_t rc = send(client->fd, buff->buff, buff->len - pos, 0);
        if(rc < 0 && errno == EINTR) {
            continue;
        }
        if(rc <= 0) {
            return -1;
        }
        pos += rc;
    }
    return 0;
}

static bool
handle_request(orka_client_t* client, int header_table_ref)
{
    orka_gil_acquire();

    lua_rawgeti(client->lua, LUA_REGISTRYINDEX, header_table_ref);
    luaL_unref(client->lua, LUA_REGISTRYINDEX, header_table_ref);
    lua_call(client->lua, 1, 3);

    fix_up_headers(client);

    {
        orka_buffer_t buff;
        orka_buffer_init(&buff, 128);
        collect_response_headers(client, &buff);
        int rc = write_buffer(client, &buff);
        orka_buffer_free(&buff);
        if(rc < 0) {
            orka_gil_release();
            return false;
        }
    }

    while(1) {
        lua_pushvalue(client->lua, -1);
        lua_call(client->lua, 0, 1);
        if(lua_isnoneornil(client->lua, -1)) {
            break;
        }

        size_t chunk_len;
        const char* chunk_ptr = luaL_checklstring(client->lua, -1, &chunk_len);

        orka_buffer_t buff;
        orka_buffer_init(&buff, chunk_len + 64);

        if(client->chunked) {
            char chunk_header[32];
            snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", chunk_len);
            orka_buffer_append_cstr(&buff, chunk_header);
        }

        orka_buffer_append(&buff, chunk_ptr, chunk_len);

        if(client->chunked) {
            orka_buffer_append_cstr(&buff, "\r\n");
        }

        int rc = write_buffer(client, &buff);
        orka_buffer_free(&buff);
        if(rc < 0) {
            orka_gil_release();
            return false;
        }

        lua_pop(client->lua, 1);
    }

    if(client->chunked) {
        orka_buffer_t end_of_chunked;
        end_of_chunked.buff = "0\r\n\r\n";
        end_of_chunked.len = strlen(end_of_chunked.buff);
        if(write_buffer(client, &end_of_chunked) < 0) {
            orka_gil_release();
            return false;
        }
    }

    orka_gil_release();
    return true;
}

void
orka_start_client(orka_client_t* client)
{
    while(true) {
        int header_table_ref = read_request(client);
        if(header_table_ref == LUA_NOREF) {
            break;
        }

        if(!handle_request(client, header_table_ref)) {
            break;
        }

        if(!client->keep_alive) {
            break;
        }
    }

    if(client->fd >= 0) {
        close(client->fd);
    }

    free(client);
}
