#include "pisa_defs.h"


void emit_word(MachineCode *mc, uint32_t word) {
    if (mc->size >= mc->capacity) {
        mc->capacity *= 2;
        mc->buffer = (uint32_t*)realloc(mc->buffer, sizeof(uint32_t) * mc->capacity);
    }
    mc->buffer[mc->size++] = word;
}

/* Type-R: [OP:8] [DEST:8] [SRC_A:8] [SRC_B:8] */
uint32_t encode_type_r(uint8_t op, uint8_t dest, uint8_t src_a, uint8_t src_b) {
    return (op << 24) | (dest << 16) | (src_a << 8) | src_b;
}

/* Type-I: [OP:8] [DEST:8] [IMM16:16] */
uint32_t encode_type_i(uint8_t op, uint8_t dest, uint16_t imm) {
    return (op << 24) | (dest << 16) | imm;
}

/* 辅助函数: 发射标量加载 */
void emit_s_load(MachineCode *mc, uint8_t s_dest, uint8_t s_base, uint8_t offset) {
    // 假设 offset 寄存器我们硬编码为 0 (直接用立即数偏移在 Type-I 不支持，这里简化为用寄存器)
    // 实际手册中 LOAD 是 Type-R: Base + OffsetReg
    // 为了模拟 S_LOAD_B32 sD, sBase, ImmOffset，我们可能需要特殊指令或先加载立即数
    
    // 简化策略：假设 SRC_B 字段如果是立即数，硬件支持小范围偏移
    emit_word(mc, encode_type_r(OP_S_LOAD_B32, s_dest, s_base, offset));
}

/* 辅助函数: 发射向量运算 */
void emit_v_add(MachineCode *mc, uint8_t v_dest, uint8_t v_src0, uint8_t v_src1) {
    emit_word(mc, encode_type_r(OP_V_ADD_F32, v_dest, v_src0, v_src1));
}

void emit_v_mul(MachineCode *mc, uint8_t v_dest, uint8_t v_src0, uint8_t v_src1) {
    emit_word(mc, encode_type_r(OP_V_MUL_F32, v_dest, v_src0, v_src1));
}