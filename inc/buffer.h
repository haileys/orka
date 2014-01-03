#ifndef ORKA_BUFFER_H
#define ORKA_BUFFER_H

typedef struct {
    char* buff;
    size_t len;
    size_t cap;
}
orka_buffer_t;

void
orka_buffer_init(orka_buffer_t*, size_t initial_cap);

void
orka_buffer_append(orka_buffer_t*, const char* str, size_t len);

void
orka_buffer_append_cstr(orka_buffer_t*, const char* str);

void
orka_buffer_free(orka_buffer_t*);

#endif
