#include <stdio.h>
#include <stdlib.h>
#include "gpu_ir.h"
#include "pisa_defs.h"
#include "gpu_linker.h"

uint8_t reg(unsigned idx, int vec) { return vec ? idx % 8 : 10 + idx % 100; }

void compile_nir_to_machine(NirShader *s, LinkerProgram *p, MachineCode *mc) {
    printf("\n=== Generating Machine Code ===\n");

    /* [修复] 增加防御性检查 */
    if (s == NULL) {
        fprintf(stderr, "Error: NirShader pointer (s) is NULL in backend!\n");
        return;
    }
    
    if (s->start_block == NULL) {
        fprintf(stderr, "Error: NirShader has no start_block!\n");
        return;
    }

    NirBlock *b = s->start_block;
    while(b) {
        NirInstr *i = b->start;
        while(i) {
            /* [修复] 增加对操作数的检查，防止空指针 */
            if (i->op == nir_intrinsic_load_var) {
                if (i->var_name) {
                    LinkerRes *r = linker_find(p, i->var_name);
                    if(r) {
                        if(r->type == RES_ATTR) {
                            emit_word(mc, encode_r(OP_V_MOV, reg(i->def.index, 1), r->phys_reg, 0));
                            printf("  V_MOV v%d, v%d\n", reg(i->def.index, 1), r->phys_reg);
                        } else {
                            emit_word(mc, encode_r(OP_S_LOAD, reg(i->def.index, 0), 0, r->offset));
                            printf("  S_LOAD s%d, s0, %d\n", reg(i->def.index, 0), r->offset);
                        }
                    } else {
                        printf("  ; Warning: Resource '%s' not found in linker\n", i->var_name);
                    }
                }
            } else if (i->op == nir_op_fadd) {
                if (i->num_srcs >= 2 && i->srcs[0].ssa && i->srcs[1].ssa) {
                    emit_word(mc, encode_r(OP_V_ADD, reg(i->def.index, 1), reg(i->srcs[0].ssa->index, 1), reg(i->srcs[1].ssa->index, 1)));
                    printf("  V_ADD v%d, v%d, v%d\n", reg(i->def.index, 1), reg(i->srcs[0].ssa->index, 1), reg(i->srcs[1].ssa->index, 1));
                } else {
                    printf("  ; Skip Invalid FADD (missing src)\n");
                }
            }
            i = i->next;
        }
        b = b->next_block;
    }
}

void dump_binary(MachineCode *mc, const char *f) {
    FILE *fp = fopen(f, "wb");
    if(fp) { 
        fwrite(mc->buffer, 4, mc->size, fp); 
        fclose(fp); 
        printf("Binary written to %s (%zu bytes)\n", f, mc->size * 4); 
    } else {
        perror("Failed to write binary");
    }
}