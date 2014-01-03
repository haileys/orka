#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lua.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

void
orka_buffer_init(orka_buffer_t* buffer, size_t initial_cap)
{
    buffer->len = 0;
    buffer->cap = initial_cap;
    buffer->buff = malloc(initial_cap);
    buffer->buff[0] = 0;
}

static void
orka_buffer_ensure_size(orka_buffer_t* buffer, size_t desired_size)
{
    if(buffer->cap < desired_size) {
        if(buffer->cap == 0) {
            buffer->cap = 4;
        }

        while(buffer->cap < desired_size) {
            buffer->cap *= 2;
        }

        buffer->buff = realloc(buffer->buff, buffer->cap);
    }
}

void
orka_buffer_append(orka_buffer_t* buffer, const char* str, size_t len)
{
    orka_buffer_ensure_size(buffer, buffer->len + len + 1);
    memcpy(buffer->buff + buffer->len, str, len);
    buffer->len += len;
    buffer->buff[buffer->len] = 0;
}

void
orka_buffer_append_cstr(orka_buffer_t* buffer, const char* str)
{
    orka_buffer_append(buffer, str, strlen(str));
}

void
orka_buffer_free(orka_buffer_t* buffer)
{
    buffer->len = 0;
    buffer->cap = 0;

    free(buffer->buff);
    buffer->buff = NULL;
}
