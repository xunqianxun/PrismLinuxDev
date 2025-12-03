/* glsl.l */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"    /* 引用 AST 定义 */
#include "glsl.tab.h"  /* 引用 Bison 生成的 Token 定义 */

/* 维护行列号 */
int current_column = 1;

void update_loc() {
    yylloc.first_line = yylloc.last_line = yylineno;
    yylloc.first_column = current_column;
    yylloc.last_column = current_column + yyleng - 1;
    current_column += yyleng;
}

/* 简单的符号表查询模拟 (Lexer Hack) */
int check_type() {
    /* 在真实编译器中，这里应该查询符号表看该标识符是否被 typedef 或 struct 定义过 */
    /* 这里为了演示，硬编码 "MyStruct" 为类型 */
    if (strcmp(yytext, "MyStruct") == 0) {
        return TYPE_NAME;
    }
    return IDENTIFIER;
}

/* 每次匹配 Token 前更新位置 */
#define YY_USER_ACTION update_loc();
%}

%option noyywrap
%option yylineno

/* 正则表达式定义 */
DIGIT       [0-9]
ID          [a-zA-Z_][a-zA-Z0-9_]*
EXP         [eE][+-]?{DIGIT}+
FLOAT       ({DIGIT}+\.{DIGIT}*{EXP}?[fF]?)|(\.{DIGIT}+{EXP}?[fF]?)|({DIGIT}+{EXP}[fF]?)
INT         {DIGIT}+[uU]?
HEX         0[xX][0-9a-fA-F]+[uU]?

%%

    /* --- 空白与注释 --- */
[ \t\r]+    { /* 忽略空白 */ }
\n          { current_column = 1; } /* 换行重置列号 */
"//".* { /* 忽略单行注释 */ }

    /* --- 预处理指令 (简化处理：忽略) --- */
^#[ \t]*version.* { /* ignore */ }
^#[ \t]*extension.* { /* ignore */ }
^#[ \t]*include.* { /* ignore */ }

    /* --- 关键字：基本类型 --- */
"void"          { return VOID; }
"bool"          { return BOOL; }
"int"           { return INT; }
"uint"          { return UINT; }
"float"         { return FLOAT; }
"double"        { return DOUBLE; }
"vec2"          { return VEC2; }
"vec3"          { return VEC3; }
"vec4"          { return VEC4; }
"ivec2"         { return IVEC2; }
"ivec3"         { return IVEC3; }
"ivec4"         { return IVEC4; }
"mat3"          { return MAT3; }
"mat4"          { return MAT4; }
"struct"        { return STRUCT; }

    /* --- 关键字：限定符 --- */
"in"            { return IN; }
"out"           { return OUT; }
"inout"         { return INOUT; }
"uniform"       { return UNIFORM; }
"const"         { return CONST; }
"layout"        { return LAYOUT; }

    /* --- 关键字：控制流 --- */
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"for"           { return FOR; }
"return"        { return RETURN; }
"discard"       { return DISCARD; }

    /* --- 字面量 --- */
"true"          { yylval.ival = 1; return BOOL_CONST; }
"false"         { yylval.ival = 0; return BOOL_CONST; }

{FLOAT}         { yylval.fval = strtof(yytext, NULL); return FLOAT_CONST; }
{INT}           { yylval.ival = (int)strtol(yytext, NULL, 0); return INT_CONST; }
{HEX}           { yylval.ival = (int)strtol(yytext, NULL, 16); return INT_CONST; }

    /* --- 运算符 --- */
"++"            { return INC_OP; }
"--"            { return DEC_OP; }
"<="            { return LE_OP; }
">="            { return GE_OP; }
"=="            { return EQ_OP; }
"!="            { return NE_OP; }
">"             { return '>'; }
"<"             { return '<'; }
"!"             { return '!'; }
"&&"            { return AND_OP; }
"||"            { return OR_OP; }
"*="            { return MUL_ASSIGN; }
"/="            { return DIV_ASSIGN; }
"+="            { return ADD_ASSIGN; }
"-="            { return SUB_ASSIGN; }
"="             { return '='; }
"+"             { return '+'; }
"-"             { return '-'; }
"*"             { return '*'; }
"/"             { return '/'; }
"("             { return '('; }
")"             { return ')'; }
"{"             { return '{'; }
"}"             { return '}'; }
"["             { return '['; }
"]"             { return ']'; }
";"             { return ';'; }
","             { return ','; }
"."             { return '.'; }

    /* --- 标识符 (Lexer Hack) --- */
{ID} {
    yylval.sval = strdup(yytext);
    return check_type();
}

. {
    fprintf(stderr, "Lexical Error: Unexpected character '%s' at line %d\n", yytext, yylineno);
    exit(1);
}

%%