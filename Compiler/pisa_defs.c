#include "pisa_defs.h"
#include <stdio.h>
#include <stdlib.h>

MachineCode* create_code_buffer() {
    MachineCode *mc = (MachineCode*)malloc(sizeof(MachineCode));
    mc->size = 0; 
    mc->capacity = 1024; /* 修复：使用 capacity */
    mc->buffer = (uint32_t*)malloc(sizeof(uint32_t) * mc->capacity);
    return mc;
}

void emit_word(MachineCode *mc, uint32_t w) {
    if (mc->size >= mc->capacity) { /* 修复：使用 capacity */
        mc->capacity *= 2;
        mc->buffer = (uint32_t*)realloc(mc->buffer, sizeof(uint32_t) * mc->capacity);
    }
    mc->buffer[mc->size++] = w;
}

uint32_t encode_r(uint8_t op, uint8_t d, uint8_t s0, uint8_t s1) {
    return (op << 24) | (d << 16) | (s0 << 8) | s1;
}