/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

#ifndef YY_YY2_SRC_PARSERS_SMTLIB2_SMTSYN_H_INCLUDED
# define YY_YY2_SRC_PARSERS_SMTLIB2_SMTSYN_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yy2debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    NUMERAL = 258,
    DECIMAL = 259,
    HEXADECIMAL = 260,
    BINARY = 261,
    STRING = 262,
    SYMBOL = 263,
    KEYWORD = 264,
    TK_EOF = 265,
    TK_BANG = 266,
    TK_UNDERSCORE = 267,
    TK_AS = 268,
    TK_ASSERT = 269,
    TK_BACKGROUND = 270,
    TK_BOOL = 271,
    TK_CHECK_SAT = 272,
    TK_CONTINUED_EXECUTION = 273,
    TK_DECLARE_FUN = 274,
    TK_DECLARE_CONST = 275,
    TK_DECLARE_SORT = 276,
    TK_DEFINE_FUN = 277,
    TK_DEFINE_SORT = 278,
    TK_DISTINCT = 279,
    TK_ECHO = 280,
    TK_ERROR = 281,
    TK_EXISTS = 282,
    TK_EXIT = 283,
    TK_FALSE = 284,
    TK_FORALL = 285,
    TK_GET_ASSERTIONS = 286,
    TK_GET_ASSIGNMENT = 287,
    TK_GET_INFO = 288,
    TK_GET_OPTION = 289,
    TK_GET_PROOF = 290,
    TK_GET_UNSAT_CORE = 291,
    TK_GET_MODEL = 292,
    TK_GET_VALUE = 293,
    TK_IMMEDIATE_EXIT = 294,
    TK_INCOMPLETE = 295,
    TK_LET = 296,
    TK_LOGIC = 297,
    TK_NONE = 298,
    TK_NUMERAL = 299,
    TK_MEMOUT = 300,
    TK_PAR = 301,
    TK_POP = 302,
    TK_PUSH = 303,
    TK_DECIMAL = 304,
    TK_SET_INFO = 305,
    TK_SET_LOGIC = 306,
    TK_SET_OPTION = 307,
    TK_STRING = 308,
    TK_THEORY = 309,
    TK_TRUE = 310,
    TK_UNSUPPORTED = 311,
    TK_PATTERN = 312,
    TK_NAMED = 313,
    TK_LAMBDA = 314
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 50 "src/parsers/smtlib2/smtsyn.y" /* yacc.c:1909  */

  bool boolean;
  unsigned int numeral;
  char * string;
  T_BIND bind;
  T_BIND_LIST bind_list;
  T_SORT sort;
  T_TERM term;
  T_TERM_LIST term_list;
  T_SORT_LIST sort_list;
  T_VAR var;                 /**< sorted variable */
  T_STACK_VAR stack_var;
  T_NUMERAL_LIST numeral_list;
  T_SYMBOL_LIST symbol_list;
  /* extension of SMT-LIB2 with macros */
  T_MACRO macro;

#line 132 "src/parsers/smtlib2/smtsyn.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yy2lval;

int yy2parse (void);

#endif /* !YY_YY2_SRC_PARSERS_SMTLIB2_SMTSYN_H_INCLUDED  */
