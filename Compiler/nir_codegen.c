#include <stdio.h>
#include "ast.h"
#include "gpu_ir.h"

typedef struct Builder { NirShader *s; NirBlock *b; } Builder;

NirDef* gen(Builder *bd, ASTNode *n) {
    if(!n) return NULL;

    switch(n->type) {
        case NODE_INT_CONST: 
        case NODE_FLOAT_CONST: { 
            NirInstr *i = nir_build_alu(bd->s, bd->b, nir_op_mov, NULL, NULL); 
            return &i->def; 
        }
        case NODE_VAR_REF: { 
            if (!n->data.str_val) return NULL;
            NirInstr *i = nir_build_load(bd->s, bd->b, n->data.str_val, 1); 
            return &i->def; 
        }
        case NODE_VAR_DECL: 
            if(n->data.var_decl.initializer) {
                NirDef *v = gen(bd, n->data.var_decl.initializer);
                if (v && n->data.var_decl.name) {
                    nir_build_store(bd->s, bd->b, n->data.var_decl.name, v, 0xF);
                }
            } 
            return NULL;
        case NODE_BINARY_EXPR:
            if(n->data.binary.op == OP_ASSIGN) {
                NirDef *v = gen(bd, n->data.binary.right);
                if (v && n->data.binary.left->type == NODE_VAR_REF) {
                    nir_build_store(bd->s, bd->b, n->data.binary.left->data.str_val, v, 0xF);
                }
                return v;
            }
            // 普通算术
            NirDef *s0 = gen(bd, n->data.binary.left);
            NirDef *s1 = gen(bd, n->data.binary.right);
            if (s0 && s1) {
                NirInstr *i = nir_build_alu(bd->s, bd->b, nir_op_fadd, s0, s1);
                return &i->def;
            }
            return NULL;
        case NODE_COMPOUND_STMT: { 
             ASTNode *c = n->next; 
             while(c){ gen(bd,c); c=c->next; } 
             return NULL; 
        }
        case NODE_EXPR_STMT: 
            gen(bd, n->next); 
            return NULL;
        case NODE_IF_STMT: {
            NirDef *c = gen(bd, n->data.if_stmt.condition);
            if (!c) return NULL;
            
            NirBlock *t = nir_create_block(bd->s);
            NirBlock *e = nir_create_block(bd->s);
            NirBlock *m = nir_create_block(bd->s);
            
            nir_build_branch(bd->b, c, t, e);
            
            // Then
            bd->b = t; 
            gen(bd, n->data.if_stmt.then_branch); 
            nir_build_jump(t, m);
            
            // Else
            bd->b = e; 
            if(n->data.if_stmt.else_branch) gen(bd, n->data.if_stmt.else_branch); 
            nir_build_jump(e, m);
            
            // Merge
            bd->b = m; 
            return NULL;
        }
        default: return NULL;
    }
}

NirShader* generate_ssa_nir(ASTNode *root) {
    printf("DEBUG: Starting generate_ssa_nir...\n");
    NirShader *s = nir_create_shader();
    if (!s) { printf("DEBUG: Failed to create shader!\n"); return NULL; }

    NirBlock *entry = nir_create_block(s);
    Builder bd = {s, entry};
    
    ASTNode *curr = root;
    while(curr) {
        if(curr->type == NODE_FUNC_DEF) {
            // 生成函数体
            gen(&bd, curr->data.func_def.body);
        } else {
            // 生成全局变量或其他
            gen(&bd, curr);
        }
        curr = curr->next;
    }
    
    nir_print_shader(s);
    printf("DEBUG: NIR Generation Done. Shader ptr: %p\n", (void*)s);
    return s;
}