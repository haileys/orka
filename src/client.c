#include <stdio.h>
#include <stdlib.h>

#include "orka.h"

static void
on_close(uv_handle_t* handle)
{
    orka_client_t* client = handle->data;

    if(client->header_table_ref != LUA_NOREF) {
        luaL_unref(client->lua, LUA_REGISTRYINDEX, client->header_table_ref);
        client->header_table_ref = LUA_NOREF;
    }

    free(client);
}

static uv_buf_t
on_alloc(uv_handle_t* handle, size_t suggested_size)
{
    (void)suggested_size;
    orka_client_t* client = handle->data;

    if(client->scratch_used) {
        fprintf(stderr, "on_alloc: trying allocate scratch buffer before it has been freed by previous user");
        abort();
    }

    return uv_buf_init(client->scratch, sizeof(client->scratch));
}

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
    orka_client_t* client = parser->data;

    if(static_append(client->url, sizeof(client->url), ptr, len)) {
        client->abort_status = "414 Request-URI Too Long";
        return -1;
    }

    return 0;
}

static void
save_header_field_value(orka_client_t* client)
{
    for(char* ptr = client->header_field; *ptr; ptr++) {
        char c = *ptr;

        if(c >= 'A' && c <= 'Z') {
            *ptr = c - 'A' + 'a';
        } else if(c == '-') {
            *ptr = '_';
        }
    }

    lua_rawgeti(client->lua, LUA_REGISTRYINDEX, client->header_table_ref);
    lua_pushstring(client->lua, client->header_value);
    lua_setfield(client->lua, -2, client->header_field);
    lua_pop(client->lua, 1);

    client->header_field[0] = 0;
    client->header_value[0] = 0;
}

static int
on_header_field(http_parser* parser, const char* ptr, size_t len)
{
    orka_client_t* client = parser->data;

    if(client->header_field[0]) {
        save_header_field_value(client);
    }

    if(static_append(client->header_field, sizeof(client->header_field), ptr, len)) {
        client->abort_status = "400 Bad Request (header field too long)";
        return -1;
    }

    return 0;
}

static int
on_header_value(http_parser* parser, const char* ptr, size_t len)
{
    orka_client_t* client = parser->data;

    if(static_append(client->header_value, sizeof(client->header_value), ptr, len)) {
        client->abort_status = "400 Bad Request (header value too long)";
        return -1;
    }

    return 0;
}

static int
on_headers_complete(http_parser* parser)
{
    orka_client_t* client = parser->data;

    if(client->header_field[0]) {
        save_header_field_value(client);
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
full_request(orka_client_t* client)
{
    lua_rawgeti(client->lua, LUA_REGISTRYINDEX, client->handler_ref);
    lua_rawgeti(client->lua, LUA_REGISTRYINDEX, client->header_table_ref);
    lua_call(client->lua, 1, 0);
}

static void
on_read(uv_stream_t* handle, ssize_t nread, uv_buf_t buff)
{
    orka_client_t* client = handle->data;

    if(nread <= 0) {
        uv_close((uv_handle_t*)handle, on_close);
        return;
    }

    http_parser_execute(&client->parser, &parser_settings, buff.base, nread);

    client->scratch_used = false;

    if(HTTP_PARSER_ERRNO(&client->parser) == HPE_OK) {
        // we haven't read the complete request headers yet, wait for more data
        return;
    }

    // at this point we've either read a complete request or we're going to
    // terminate the request
    uv_read_stop((uv_stream_t*)&client->client);

    if(HTTP_PARSER_ERRNO(&client->parser) == HPE_CB_headers_complete) {
        full_request(client);
        return;
    }
}

void
orka_start_client(orka_client_t* client)
{
    client->scratch_used = false;
    client->abort_status = NULL;
    client->url[0] = 0;
    client->header_field[0] = 0;
    client->header_value[0] = 0;

    if(client->header_table_ref != LUA_NOREF) {
        luaL_unref(client->lua, LUA_REGISTRYINDEX, client->header_table_ref);
    }

    lua_newtable(client->lua);
    client->header_table_ref = luaL_ref(client->lua, LUA_REGISTRYINDEX);

    uv_read_start((uv_stream_t*)&client->client, on_alloc, on_read);
}
