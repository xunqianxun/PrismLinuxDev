#ifndef GPU_IR_H
#define GPU_IR_H

#include <stdint.h>
#include <stdbool.h>

/* --- 基础类型定义 --- */

typedef enum {
    /* ALU 运算 (带类型大小) */
    nir_op_fadd, nir_op_fsub, nir_op_fmul, nir_op_fdiv,
    nir_op_iadd, nir_op_isub, nir_op_imul,
    nir_op_fmax, nir_op_fmin,
    nir_op_fsin, nir_op_fcos,
    
    /* 移动与修饰 */
    nir_op_mov,
    nir_op_vec2, nir_op_vec3, nir_op_vec4, /* 构造向量 */

    /* 内存/变量操作 (Intrinsic) */
    nir_intrinsic_load_var,
    nir_intrinsic_store_var,
    
    /* 控制流 */
    nir_jump,
    nir_branch /* 条件跳转 */
} NirOp;

/* SSA 定义 (Definition)
 * 代表一条指令产生的结果。在 SSA 中，每个结果都是唯一的 ID。
 */
typedef struct NirDef {
    unsigned index;         // 全局唯一的 SSA ID (例如 %1, %2)
    uint8_t num_components; // 向量分量数 (1=scalar, 3=vec3)
    uint8_t bit_size;       // 位宽 (32 for float/int)
    
    // 简单的链表用于以后做 Use-Def 链分析
    struct NirInstr *parent_instr; 
} NirDef;

/* SSA 来源 (Source) 
 * 代表指令的操作数。包含 Swizzle (混洗) 信息。
 */
typedef struct NirSrc {
    struct NirDef *ssa;     // 指向定义该值的指令结果
    
    /* GPU 核心特性：Swizzle
     * swizzle[0] = 0 表示取 .x
     * swizzle[1] = 2 表示取 .z
     * 例如 src.yxzw -> {1, 0, 2, 3}
     */
    uint8_t swizzle[4]; 
    bool negate;            // -src
    bool abs;               // abs(src)
} NirSrc;

/* 指令基类 */
typedef struct NirInstr {
    NirOp op;
    struct NirBlock *block; // 所属的基本块
    struct NirInstr *next;
    struct NirInstr *prev;
    
    /* 结果 (Destination) */
    /* Store/Branch 指令没有 SSA 结果，但 ALU 有 */
    NirDef def; 

    /* 操作数 */
    int num_srcs;
    NirSrc srcs[4]; // 大多数指令最多4个操作数

    /* 对于 Store 指令，我们需要 WriteMask (写掩码)
     * e.g., variable.xz = ... (mask = 0b0101)
     */
    uint8_t write_mask; 

    /* 对于 Load/Store 指令，需要指向变量名 */
    char *var_name;
} NirInstr;

/* 基本块 (Basic Block)
 * 包含指令列表，以及控制流图的边 (Edges)
 */
typedef struct NirBlock {
    unsigned index; // 块 ID (Label)
    
    NirInstr *start;
    NirInstr *end;

    /* 控制流图 (CFG) */
    struct NirBlock *successors[2]; // 后继块 (If True, If False)
    
    struct NirBlock *next_block; // 线性布局的下一个块 (用于打印顺序)
} NirBlock;

/* 函数 / Shader */
typedef struct NirShader {
    NirBlock *start_block;
    unsigned num_ssa_defs; // 计数器，用于生成唯一 ID
    unsigned num_blocks;
} NirShader;

/* --- API --- */
NirShader* nir_create_shader();
NirBlock* nir_create_block(NirShader *shader);
NirInstr* nir_build_alu(NirShader *shader, NirBlock *block, NirOp op, NirDef *src0, NirDef *src1);
NirInstr* nir_build_load(NirShader *shader, NirBlock *block, char *var_name, int num_comp);
void nir_build_store(NirShader *shader, NirBlock *block, char *var_name, NirDef *value, uint8_t mask);
void nir_build_jump(NirBlock *from, NirBlock *to);
void nir_build_branch(NirBlock *from, NirDef *cond, NirBlock *then_block, NirBlock *else_block);

void nir_print_shader(NirShader *shader);

#endif