#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symbol_table.h"

DataType resolve_type_from_string(const char *type_str) {
    if (strcmp(type_str, "int") == 0) return DT_INT;
    if (strcmp(type_str, "float") == 0) return DT_FLOAT;
    if (strcmp(type_str, "bool") == 0) return DT_BOOL;
    if (strcmp(type_str, "vec3") == 0) return DT_VEC3;
    if (strcmp(type_str, "vec4") == 0) return DT_VEC4;
    if (strcmp(type_str, "void") == 0) return DT_VOID;
    return DT_UNKNOWN; 
}

/* [Fixed] Removed duplicate definition of get_datatype_name. 
   It is linked from ast.c */

void analyze_node(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_TRANSLATION_UNIT:
        case NODE_COMPOUND_STMT: {
            if (node->type == NODE_COMPOUND_STMT) enter_scope();
            ASTNode *current = (node->type == NODE_COMPOUND_STMT) ? node->next : node;
            
            while (current) {
                if (current->type == NODE_TRANSLATION_UNIT) {
                     ASTNode *item = current;
                     while(item && item->type == NODE_TRANSLATION_UNIT) {
                         if (item->data.func_def.body) analyze_node(item->data.func_def.body);
                         item = item->next;
                     }
                } else {
                    analyze_node(current);
                }
                
                if (node->type == NODE_COMPOUND_STMT) current = current->next;
                else break;
            }
            if (node->type == NODE_COMPOUND_STMT) exit_scope();
            break;
        }
        case NODE_VAR_DECL: {
            char *type_str = node->data.var_decl.type->data.str_val;
            DataType decl_type = resolve_type_from_string(type_str);
            if (!define_symbol(node->data.var_decl.name, decl_type)) {
                fprintf(stderr, "Semantic Warning: Variable '%s' redefinition.\n", node->data.var_decl.name);
            }
            node->data_type = decl_type;
            if (node->data.var_decl.initializer) analyze_node(node->data.var_decl.initializer);
            break;
        }
        case NODE_FUNC_DEF: {
            char *ret_type = node->data.func_def.return_type->data.str_val;
            define_symbol(node->data.func_def.name, resolve_type_from_string(ret_type));
            enter_scope();
            analyze_node(node->data.func_def.body);
            exit_scope();
            break;
        }
        case NODE_VAR_REF: {
            Symbol *sym = lookup_symbol(node->data.str_val);
            if (sym) node->data_type = sym->type;
            else node->data_type = DT_ERROR;
            break;
        }
        case NODE_BINARY_EXPR: {
            analyze_node(node->data.binary.left);
            analyze_node(node->data.binary.right);
            node->data_type = node->data.binary.left->data_type; 
            break;
        }
        case NODE_INT_CONST: node->data_type = DT_INT; break;
        case NODE_FLOAT_CONST: node->data_type = DT_FLOAT; break;
        case NODE_EXPR_STMT: analyze_node(node->next); break;
        case NODE_IF_STMT: 
            analyze_node(node->data.if_stmt.condition);
            analyze_node(node->data.if_stmt.then_branch);
            if(node->data.if_stmt.else_branch) analyze_node(node->data.if_stmt.else_branch);
            break;
        default: break;
    }
}

void semantic_analysis(ASTNode *root) {
    init_symbol_table();
    ASTNode *curr = root;
    while(curr) {
        analyze_node(curr);
        curr = curr->next;
    }
}