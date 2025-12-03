/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_GLSL_TAB_H_INCLUDED
# define YY_YY_GLSL_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    IDENTIFIER = 258,              /* IDENTIFIER  */
    TYPE_NAME = 259,               /* TYPE_NAME  */
    FLOAT_CONST = 260,             /* FLOAT_CONST  */
    INT_CONST = 261,               /* INT_CONST  */
    BOOL_CONST = 262,              /* BOOL_CONST  */
    VOID = 263,                    /* VOID  */
    BOOL = 264,                    /* BOOL  */
    INT = 265,                     /* INT  */
    UINT = 266,                    /* UINT  */
    FLOAT = 267,                   /* FLOAT  */
    DOUBLE = 268,                  /* DOUBLE  */
    VEC2 = 269,                    /* VEC2  */
    VEC3 = 270,                    /* VEC3  */
    VEC4 = 271,                    /* VEC4  */
    IVEC2 = 272,                   /* IVEC2  */
    IVEC3 = 273,                   /* IVEC3  */
    IVEC4 = 274,                   /* IVEC4  */
    MAT2 = 275,                    /* MAT2  */
    MAT3 = 276,                    /* MAT3  */
    MAT4 = 277,                    /* MAT4  */
    STRUCT = 278,                  /* STRUCT  */
    IN = 279,                      /* IN  */
    OUT = 280,                     /* OUT  */
    INOUT = 281,                   /* INOUT  */
    UNIFORM = 282,                 /* UNIFORM  */
    CONST = 283,                   /* CONST  */
    LAYOUT = 284,                  /* LAYOUT  */
    IF = 285,                      /* IF  */
    ELSE = 286,                    /* ELSE  */
    WHILE = 287,                   /* WHILE  */
    FOR = 288,                     /* FOR  */
    RETURN = 289,                  /* RETURN  */
    DISCARD = 290,                 /* DISCARD  */
    INC_OP = 291,                  /* INC_OP  */
    DEC_OP = 292,                  /* DEC_OP  */
    LE_OP = 293,                   /* LE_OP  */
    GE_OP = 294,                   /* GE_OP  */
    EQ_OP = 295,                   /* EQ_OP  */
    NE_OP = 296,                   /* NE_OP  */
    AND_OP = 297,                  /* AND_OP  */
    OR_OP = 298,                   /* OR_OP  */
    XOR_OP = 299,                  /* XOR_OP  */
    MUL_ASSIGN = 300,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 301,              /* DIV_ASSIGN  */
    ADD_ASSIGN = 302,              /* ADD_ASSIGN  */
    SUB_ASSIGN = 303,              /* SUB_ASSIGN  */
    LEFT_OP = 304,                 /* LEFT_OP  */
    RIGHT_OP = 305,                /* RIGHT_OP  */
    LOWER_THAN_ELSE = 306          /* LOWER_THAN_ELSE  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 33 "glsl.y"
 
    int ival; 
    float fval; 
    char *sval; 
    struct ASTNode *node; 

#line 122 "glsl.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;
extern YYLTYPE yylloc;

int yyparse (void);


#endif /* !YY_YY_GLSL_TAB_H_INCLUDED  */
