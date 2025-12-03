#ifndef AST_H
#define AST_H

/* 1. GLSL Data Types */
typedef enum {
    DT_UNKNOWN = 0,
    DT_VOID,
    DT_INT,
    DT_FLOAT,
    DT_BOOL,
    DT_VEC2,
    DT_VEC3,
    DT_VEC4,
    DT_STRUCT,
    DT_ERROR
} DataType;

/* 2. AST Node Types */
typedef enum {
    NODE_TRANSLATION_UNIT,
    NODE_FUNC_DEF,
    NODE_VAR_DECL,
    NODE_PARAM_DECL,
    NODE_TYPE_SPECIFIER,
    NODE_STRUCT_DEF,
    NODE_COMPOUND_STMT,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_FOR_STMT,
    NODE_RETURN_STMT,
    NODE_EXPR_STMT,
    NODE_BINARY_EXPR,
    NODE_UNARY_EXPR,
    NODE_FUNC_CALL,
    NODE_VAR_REF,
    NODE_MEMBER_ACCESS,
    NODE_INT_CONST,
    NODE_FLOAT_CONST,
    NODE_BOOL_CONST
} NodeType;

/* 3. Operators */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, 
    OP_ASSIGN, OP_EQ, OP_NE, OP_GT, OP_LT
} OperatorType;

/* 4. AST Node Structure */
typedef struct ASTNode {
    NodeType type;
    struct ASTNode *next; 
    DataType data_type; 

    union {
        struct {
            struct ASTNode *return_type;
            char *name;
            struct ASTNode *params;
            struct ASTNode *body;
        } func_def;

        struct {
            struct ASTNode *type;
            char *name;
            struct ASTNode *initializer;
        } var_decl;

        struct {
            OperatorType op;
            struct ASTNode *left;
            struct ASTNode *right;
        } binary;

        struct {
            struct ASTNode *condition;
            struct ASTNode *then_branch;
            struct ASTNode *else_branch;
        } if_stmt;

        struct {
            char *name;
            struct ASTNode *args;
        } func_call;

        int int_val;
        float float_val;
        char *str_val;
    } data;
} ASTNode;

/* Function Prototypes */
ASTNode* create_node(NodeType type);
ASTNode* create_int_const(int val);
ASTNode* create_float_const(float val);
ASTNode* create_var_ref(char *name);
ASTNode* create_binary_expr(OperatorType op, ASTNode *left, ASTNode *right);
ASTNode* create_func_def(ASTNode *ret_type, char *name, ASTNode *params, ASTNode *body);
ASTNode* create_if_stmt(ASTNode *cond, ASTNode *then_b, ASTNode *else_b);
ASTNode* append_node(ASTNode *list, ASTNode *new_node);
const char* get_datatype_name(DataType dt);

#endif