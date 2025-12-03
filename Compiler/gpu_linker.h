#ifndef GPU_LINKER_H
#define GPU_LINKER_H

#include "ast.h"

typedef enum { RES_ATTR, RES_UNIFORM } ResType;

typedef struct LinkerRes {
    char name[64];
    ResType type;
    int offset;
    int phys_reg;
    struct LinkerRes *next;
} LinkerRes;

/* 必须定义这个结构体，glsl.y 才能识别 LinkerProgram 类型 */
typedef struct LinkerProgram {
    LinkerRes *resources;
} LinkerProgram;

LinkerProgram* linker_create();
void linker_add(LinkerProgram *p, ASTNode *root);
int linker_link(LinkerProgram *p);
LinkerRes* linker_find(LinkerProgram *p, const char *name);
void linker_print(LinkerProgram *p);

#endif