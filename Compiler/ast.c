#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* create_node(NodeType type) {
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) { fprintf(stderr, "Out of memory\n"); exit(1); }
    node->type = type;
    node->next = NULL;
    node->data_type = DT_UNKNOWN;
    memset(&node->data, 0, sizeof(node->data));
    return node;
}

/* [Fix] Ensure this function exists */
ASTNode* create_int_const(int val) {
    ASTNode *node = create_node(NODE_INT_CONST);
    node->data.int_val = val;
    return node;
}

ASTNode* create_float_const(float val) {
    ASTNode *node = create_node(NODE_FLOAT_CONST);
    node->data.float_val = val;
    return node;
}

ASTNode* create_var_ref(char *name) {
    ASTNode *node = create_node(NODE_VAR_REF);
    node->data.str_val = strdup(name); //作用是给这个字符串做一个内存并将字符串存入然后将指针返回
    return node;
}

ASTNode* create_binary_expr(OperatorType op, ASTNode *left, ASTNode *right) {
    ASTNode *node = create_node(NODE_BINARY_EXPR);
    node->data.binary.op = op;
    node->data.binary.left = left;
    node->data.binary.right = right;
    return node;
}

ASTNode* create_func_def(ASTNode *ret_type, char *name, ASTNode *params, ASTNode *body) {
    ASTNode *node = create_node(NODE_FUNC_DEF);
    node->data.func_def.return_type = ret_type;
    node->data.func_def.name = strdup(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    return node;
}

ASTNode* create_if_stmt(ASTNode *cond, ASTNode *then_b, ASTNode *else_b) {
    ASTNode *node = create_node(NODE_IF_STMT);
    node->data.if_stmt.condition = cond;
    node->data.if_stmt.then_branch = then_b;
    node->data.if_stmt.else_branch = else_b;
    return node;
}

ASTNode* append_node(ASTNode *list, ASTNode *new_node) {
    if (!list) return new_node;
    if (!new_node) return list;
    ASTNode *current = list;
    while (current->next) current = current->next;
    current->next = new_node;
    return list;
}

const char* get_datatype_name(DataType dt) {
    switch(dt) {
        case DT_INT: return "int";
        case DT_FLOAT: return "float";
        case DT_BOOL: return "bool";
        case DT_VEC3: return "vec3";
        case DT_VEC4: return "vec4";
        case DT_VOID: return "void";
        default: return "unknown";
    }
}