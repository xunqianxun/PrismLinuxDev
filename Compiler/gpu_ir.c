#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_ir.h"

NirShader* nir_create_shader() {
    NirShader *s = (NirShader*)malloc(sizeof(NirShader));
    s->num_ssa_defs = 0;
    s->num_blocks = 0;
    s->start_block = NULL;
    return s;
}

NirBlock* nir_create_block(NirShader *shader) {
    NirBlock *b = (NirBlock*)malloc(sizeof(NirBlock));
    b->index = shader->num_blocks++;
    b->start = NULL;
    b->end = NULL;
    b->successors[0] = NULL;
    b->successors[1] = NULL;
    b->next_block = NULL;
    
    // 简单的链表插入，维护块的线性顺序
    if (shader->start_block == NULL) {
        shader->start_block = b;
    } else {
        NirBlock *curr = shader->start_block;
        while(curr->next_block) curr = curr->next_block;
        curr->next_block = b;
    }
    return b;
}

/* 将指令追加到块末尾 */
void block_append_instr(NirBlock *block, NirInstr *instr) {
    instr->block = block;
    instr->prev = block->end;
    instr->next = NULL;
    if (block->end) {
        block->end->next = instr;
    } else {
        block->start = instr;
    }
    block->end = instr;
}

/* 初始化 SSA 定义 */
void nir_def_init(NirShader *shader, NirInstr *instr, int num_comp) {
    instr->def.index = ++shader->num_ssa_defs;
    instr->def.num_components = num_comp;
    instr->def.bit_size = 32;
    instr->def.parent_instr = instr;
}

/* 构建 ALU 指令 (如 ADD, MUL) */
NirInstr* nir_build_alu(NirShader *shader, NirBlock *block, NirOp op, NirDef *src0, NirDef *src1) {
    NirInstr *instr = (NirInstr*)malloc(sizeof(NirInstr));
    memset(instr, 0, sizeof(NirInstr));
    instr->op = op;
    
    // 设置操作数
    if (src0) {
        instr->num_srcs++;
        instr->srcs[0].ssa = src0;
        // 默认 Swizzle xyz
        for(int i=0; i<4; i++) instr->srcs[0].swizzle[i] = i; 
    }
    if (src1) {
        instr->num_srcs++;
        instr->srcs[1].ssa = src1;
        for(int i=0; i<4; i++) instr->srcs[1].swizzle[i] = i;
    }

    // 结果分量数通常等于第一个操作数的分量数 (简化逻辑)
    int comps = src0 ? src0->num_components : 1;
    nir_def_init(shader, instr, comps);
    
    block_append_instr(block, instr);
    return instr;
}

/* 构建 Load 变量 */
NirInstr* nir_build_load(NirShader *shader, NirBlock *block, char *var_name, int num_comp) {
    NirInstr *instr = (NirInstr*)malloc(sizeof(NirInstr));
    memset(instr, 0, sizeof(NirInstr));
    instr->op = nir_intrinsic_load_var;
    instr->var_name = strdup(var_name);
    
    nir_def_init(shader, instr, num_comp);
    block_append_instr(block, instr);
    return instr;
}

/* 构建 Store 变量 */
void nir_build_store(NirShader *shader, NirBlock *block, char *var_name, NirDef *value, uint8_t mask) {
    NirInstr *instr = (NirInstr*)malloc(sizeof(NirInstr));
    memset(instr, 0, sizeof(NirInstr));
    instr->op = nir_intrinsic_store_var;
    instr->var_name = strdup(var_name);
    instr->write_mask = mask; // 例如 0xF (1111) 写全部
    
    // Store 指令消费一个 Source，但没有 Def (不产生 SSA 值)
    instr->num_srcs = 1;
    instr->srcs[0].ssa = value;
    for(int i=0; i<4; i++) instr->srcs[0].swizzle[i] = i;
    
    // Def index 为 0 表示无返回值
    instr->def.index = 0; 
    
    block_append_instr(block, instr);
}

void nir_build_branch(NirBlock *from, NirDef *cond, NirBlock *then_block, NirBlock *else_block) {
    NirInstr *instr = (NirInstr*)malloc(sizeof(NirInstr));
    memset(instr, 0, sizeof(NirInstr));
    instr->op = nir_branch;
    
    instr->num_srcs = 1;
    instr->srcs[0].ssa = cond;
    
    from->successors[0] = then_block;
    from->successors[1] = else_block;
    
    block_append_instr(from, instr);
}

void nir_build_jump(NirBlock *from, NirBlock *to) {
    NirInstr *instr = (NirInstr*)malloc(sizeof(NirInstr));
    memset(instr, 0, sizeof(NirInstr));
    instr->op = nir_jump;
    
    from->successors[0] = to;
    
    block_append_instr(from, instr);
}

/* --- 打印逻辑 --- */
const char* nir_op_name(NirOp op) {
    switch(op) {
        case nir_op_fadd: return "fadd";
        case nir_op_fmul: return "fmul";
        case nir_op_mov:  return "mov";
        case nir_intrinsic_load_var: return "load_var";
        case nir_intrinsic_store_var: return "store_var";
        case nir_branch: return "br";
        case nir_jump: return "jump";
        default: return "unknown";
    }
}

void nir_print_src(NirSrc *src) {
    printf("%%ssa_%d", src->ssa->index);
    // 只有当 swizzle 不是默认的 xyzw (0123) 时才打印，为了简洁
    // 这里简单起见省略打印 swizzle 逻辑
}

void nir_print_instr(NirInstr *instr) {
    printf("    ");
    if (instr->def.index != 0) {
        // 打印结果: %1 (vec3) = ...
        printf("%%ssa_%d (v%d) = ", instr->def.index, instr->def.num_components);
    }
    
    printf("%s ", nir_op_name(instr->op));
    
    if (instr->var_name) {
        printf("%s ", instr->var_name);
        if (instr->write_mask) printf("(mask:0x%x) ", instr->write_mask);
    }
    
    for (int i=0; i < instr->num_srcs; i++) {
        if (i > 0) printf(", ");
        nir_print_src(&instr->srcs[i]);
    }
    printf("\n");
    
    if (instr->op == nir_branch) {
        printf("      -> True: B%d, False: B%d\n", 
            instr->block->successors[0]->index, 
            instr->block->successors[1]->index);
    }
    else if (instr->op == nir_jump) {
        printf("      -> B%d\n", instr->block->successors[0]->index);
    }
}

void nir_print_shader(NirShader *shader) {
    printf("\n=== NIR SSA IR ===\n");
    NirBlock *curr = shader->start_block;
    while (curr) {
        printf("Block B%d:\n", curr->index);
        NirInstr *instr = curr->start;
        while (instr) {
            nir_print_instr(instr);
            instr = instr->next;
        }
        curr = curr->next_block;
    }
    printf("==================\n");
}