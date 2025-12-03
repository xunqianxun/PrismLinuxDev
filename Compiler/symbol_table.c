#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h" /* 包含头文件以获取 Scope/Symbol 定义 */

/* 全局当前作用域指针 */
static Scope *current_scope = NULL;

void init_symbol_table() {
    current_scope = NULL;
    enter_scope(); /* 创建全局作用域 */
}

void enter_scope() {
    Scope *new_scope = (Scope*)malloc(sizeof(Scope));
    if (!new_scope) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    new_scope->symbols = NULL;
    new_scope->parent = current_scope;
    current_scope = new_scope;
}

void exit_scope() {
    if (current_scope) {
        Scope *parent = current_scope->parent;
        
        /* 简单释放当前作用域的符号内存 */
        Symbol *s = current_scope->symbols;
        while (s) {
            Symbol *next = s->next;
            free(s->name);
            free(s);
            s = next;
        }
        free(current_scope);
        
        current_scope = parent;
    }
}

int define_symbol(char *name, DataType type) {
    /* 1. 检查当前作用域是否已经定义 */
    Symbol *s = current_scope->symbols;
    while (s) {
        if (strcmp(s->name, name) == 0) {
            return 0; /* 错误：重复定义 */
        }
        s = s->next;
    }

    /* 2. 如果没定义，加入链表头 */
    Symbol *new_sym = (Symbol*)malloc(sizeof(Symbol));
    if (!new_sym) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    new_sym->name = strdup(name);
    new_sym->type = type;
    new_sym->next = current_scope->symbols;
    current_scope->symbols = new_sym;
    
    return 1;
}

Symbol* lookup_symbol(char *name) {
    /* 从当前作用域向父作用域查找 */
    Scope *scope = current_scope;
    while (scope) {
        Symbol *s = scope->symbols;
        while (s) {
            if (strcmp(s->name, name) == 0) {
                return s;
            }
            s = s->next;
        }
        scope = scope->parent;
    }
    return NULL; /* 未找到 */
}