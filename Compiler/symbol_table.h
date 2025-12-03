#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include "ast.h" /* 必须包含这个，因为用到了 DataType */

/* --- 结构体定义移到这里，让 .c 文件能看到 --- */

/* 符号定义 */
typedef struct Symbol {
    char *name;
    DataType type;
    struct Symbol *next; /* 链表：同一作用域下的下一个符号 */
} Symbol;

/* 作用域定义 */
typedef struct Scope {
    Symbol *symbols;     /* 当前作用域的符号链表 */
    struct Scope *parent;/* 父作用域 */
} Scope;

/* --- 函数声明 --- */
void init_symbol_table();
void enter_scope();
void exit_scope();
int define_symbol(char *name, DataType type);
Symbol* lookup_symbol(char *name);

#endif