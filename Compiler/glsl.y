%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "pisa_defs.h"
#include "gpu_ir.h"
#include "gpu_linker.h"

extern int yylex();
extern FILE* yyin;
/* [修复] 声明 yylineno，Flex 会维护这个变量 */
extern int yylineno;

void yyerror(const char *s);
ASTNode *root = NULL;

NirShader* generate_ssa_nir(ASTNode *root);
void semantic_analysis(ASTNode *root);
void compile_nir_to_machine(NirShader *shader, LinkerProgram *prog, MachineCode *mc);
void dump_binary(MachineCode *mc, const char *filename);

ASTNode* create_type_node(const char* name) {
    ASTNode* node = create_node(NODE_TYPE_SPECIFIER);
    node->data.str_val = strdup(name);
    return node;
}
%}

/* 开启位置追踪 */
%locations

%union { 
    int ival; 
    float fval; 
    char *sval; 
    struct ASTNode *node; 
}

/* --- Token 定义 (补全所有 Flex 中用到的 Token) --- */
%token <sval> IDENTIFIER TYPE_NAME
%token <fval> FLOAT_CONST
%token <ival> INT_CONST BOOL_CONST

/* 基础类型 */
%token VOID BOOL INT UINT FLOAT DOUBLE
%token VEC2 VEC3 VEC4 
%token IVEC2 IVEC3 IVEC4
%token MAT2 MAT3 MAT4
%token STRUCT

/* 限定符 */
%token IN OUT INOUT UNIFORM CONST LAYOUT

/* 控制流 */
%token IF ELSE WHILE FOR RETURN DISCARD

/* 运算符 */
%token INC_OP DEC_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP XOR_OP
%token MUL_ASSIGN DIV_ASSIGN ADD_ASSIGN SUB_ASSIGN
%token LEFT_OP RIGHT_OP

/* 优先级定义 */
%right '=' MUL_ASSIGN DIV_ASSIGN ADD_ASSIGN SUB_ASSIGN
%left OR_OP
%left AND_OP
%left EQ_OP NE_OP
%left '<' '>' LE_OP GE_OP
%left '+' '-'
%left '*' '/'
%right INC_OP DEC_OP '!'
%left '.' '[' ']' '(' ')'

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

/* --- 类型绑定 --- */
%type <node> translation_unit external_declaration function_definition declaration
%type <node> statement compound_statement statement_list expression assignment_expression
%type <node> additive_expression multiplicative_expression primary_expression
%type <node> type_specifier fully_specified_type init_declarator_list single_declaration type_qualifier

%%

/* ================= 语法规则 ================= */

translation_unit 
    : external_declaration { root = $1; $$ = root; } 
    | translation_unit external_declaration { $$ = append_node($1, $2); } 
    ;

external_declaration 
    : function_definition { $$ = $1; } 
    | declaration { $$ = $1; } 
    ;

function_definition 
    : fully_specified_type IDENTIFIER '(' ')' compound_statement { 
        $$ = create_func_def($1, $2, NULL, $5); 
        free($2); 
    } 
    ;

declaration 
    : init_declarator_list ';' { $$ = $1; } 
    ;

init_declarator_list 
    : single_declaration { $$ = $1; } 
    ;

single_declaration 
    : fully_specified_type IDENTIFIER { 
        ASTNode* n = create_node(NODE_VAR_DECL); 
        n->data.var_decl.type = $1; 
        n->data.var_decl.name = $2; 
        $$ = n; 
    }
    | fully_specified_type IDENTIFIER '=' expression { 
        ASTNode* n = create_node(NODE_VAR_DECL); 
        n->data.var_decl.type = $1; 
        n->data.var_decl.name = $2; 
        n->data.var_decl.initializer = $4; 
        $$ = n; 
    } 
    ;

fully_specified_type 
    : type_specifier { $$ = $1; } 
    | type_qualifier type_specifier { $$ = $2; } 
    ;

type_qualifier 
    : UNIFORM { $$ = NULL; } 
    | IN { $$ = NULL; } 
    | OUT { $$ = NULL; } 
    | CONST { $$ = NULL; } 
    ;

type_specifier 
    : VOID { $$ = create_type_node("void"); } 
    | FLOAT { $$ = create_type_node("float"); } 
    | INT { $$ = create_type_node("int"); } 
    | VEC2 { $$ = create_type_node("vec2"); } 
    | VEC3 { $$ = create_type_node("vec3"); } 
    | VEC4 { $$ = create_type_node("vec4"); } 
    | MAT4 { $$ = create_type_node("mat4"); }
    /* 其他类型暂略，防止 AST 构造函数过于复杂 */
    ;

statement 
    : compound_statement { $$ = $1; } 
    | expression ';' { ASTNode* n = create_node(NODE_EXPR_STMT); n->next = $1; $$ = n; } 
    | IF '(' expression ')' statement ELSE statement { $$ = create_if_stmt($3, $5, $7); } 
    | declaration { $$ = $1; } 
    | RETURN expression ';' { $$ = create_node(NODE_RETURN_STMT); /* 简化处理 */ }
    | RETURN ';' { $$ = create_node(NODE_RETURN_STMT); }
    ;

compound_statement 
    : '{' '}' { $$ = create_node(NODE_COMPOUND_STMT); } 
    | '{' statement_list '}' { ASTNode* n = create_node(NODE_COMPOUND_STMT); n->next = $2; $$ = n; } 
    ;

statement_list 
    : statement { $$ = $1; } 
    | statement_list statement { $$ = append_node($1, $2); } 
    ;

expression 
    : assignment_expression { $$ = $1; } 
    ;

assignment_expression 
    : additive_expression { $$ = $1; } 
    | primary_expression '=' assignment_expression { 
        $$ = create_binary_expr(OP_ASSIGN, $1, $3); 
    } 
    ;

additive_expression 
    : multiplicative_expression { $$ = $1; } 
    | additive_expression '+' multiplicative_expression { $$ = create_binary_expr(OP_ADD, $1, $3); } 
    | additive_expression '-' multiplicative_expression { $$ = create_binary_expr(OP_SUB, $1, $3); } 
    ;

multiplicative_expression 
    : primary_expression { $$ = $1; } 
    | multiplicative_expression '*' primary_expression { $$ = create_binary_expr(OP_MUL, $1, $3); } 
    | multiplicative_expression '/' primary_expression { $$ = create_binary_expr(OP_DIV, $1, $3); } 
    ;

primary_expression 
    : IDENTIFIER { $$ = create_var_ref($1); free($1); } 
    | INT_CONST { $$ = create_int_const($1); } 
    | FLOAT_CONST { $$ = create_float_const($1); } 
    | BOOL_CONST { $$ = create_int_const($1); }
    | '(' expression ')' { $$ = $2; }
    ;

%%

void yyerror(const char *s) { fprintf(stderr, "Parse Error: %s line %d\n", s, yylineno); }

int main(int argc, char **argv) {
    if (argc > 1) yyin = fopen(argv[1], "r");
    if (yyparse() == 0) {
        printf("1. Parsing Successful!\n");
        
        semantic_analysis(root);
        
        printf("2. Linking...\n");
        LinkerProgram *lp = linker_create(); 
        linker_add(lp, root); 
        linker_link(lp); 
        linker_print(lp);
        
        printf("3. NIR Generation...\n");
        NirShader *ns = generate_ssa_nir(root);
        
        if (ns) {
            printf("4. Backend CodeGen...\n");
            MachineCode *mc = create_code_buffer();
            compile_nir_to_machine(ns, lp, mc);
            dump_binary(mc, "shader.bin");
        }
    }
    return 0;
}