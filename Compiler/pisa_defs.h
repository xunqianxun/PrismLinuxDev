#ifndef PISA_DEFS_H
#define PISA_DEFS_H
#include <stdint.h>
#include <stdlib.h>

/* 修复：统一使用短命名，与 backend.c 保持一致 */
#define OP_S_LOAD 0x42
#define OP_V_ADD  0x82
#define OP_V_MOV  0xC0
#define OP_S_MOV  0x40
#define OP_V_MUL  0x8A

/* 机器码缓冲区 */
typedef struct MachineCode {
    uint32_t *buffer;
    size_t size;
    size_t capacity; /* [修复] 从 cap 改为 capacity 以匹配 pisa_defs.c */
} MachineCode;

MachineCode* create_code_buffer();
void emit_word(MachineCode *mc, uint32_t w);
uint32_t encode_r(uint8_t op, uint8_t d, uint8_t s0, uint8_t s1);

#endif