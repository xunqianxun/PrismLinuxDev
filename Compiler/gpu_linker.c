#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_linker.h"

LinkerProgram* linker_create() {
    return (LinkerProgram*)calloc(1, sizeof(LinkerProgram));
}

void collect(LinkerProgram *p, ASTNode *n) {
    if (!n) return;
    if (n->type == NODE_VAR_DECL) {
        char *name = n->data.var_decl.name;
        ResType t = -1;
        if (strncmp(name, "u_", 2) == 0) t = RES_UNIFORM;
        else if (strncmp(name, "v_", 2) == 0) t = RES_ATTR;
        
        if (t != -1) {
            LinkerRes *r = (LinkerRes*)calloc(1, sizeof(LinkerRes));
            strcpy(r->name, name); 
            r->type = t; 
            r->next = p->resources; 
            p->resources = r;
        }
    }
}

void linker_add(LinkerProgram *p, ASTNode *root) {
    ASTNode *c = root; 
    while (c) { collect(p, c); c = c->next; }
}

int linker_link(LinkerProgram *p) {
    int off = 0, vgpr = 0;
    LinkerRes *r = p->resources;
    while (r) { 
        if (r->type == RES_ATTR) { 
            r->phys_reg = vgpr++; 
            r->offset = -1; 
        } else { 
            r->offset = off; 
            off += 16; 
            r->phys_reg = -1; 
        } 
        r = r->next; 
    }
    return 1;
}

LinkerRes* linker_find(LinkerProgram *p, const char *n) {
    LinkerRes *r = p->resources;
    while (r) { 
        if (!strcmp(r->name, n)) return r; 
        r = r->next; 
    }
    return NULL;
}

void linker_print(LinkerProgram *p) {
    printf("\n=== Linker Layout ===\n");
    LinkerRes *r = p->resources;
    while (r) { 
        printf("Res %s: Off %d Reg %d\n", r->name, r->offset, r->phys_reg); 
        r = r->next; 
    }
    printf("=====================\n");
}