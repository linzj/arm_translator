%{
#include <stdlib.h>
#include <stdio.h>
#include "IRContext.h"
%}
%pure-parser
%parse-param {struct IRContext* context}
%lex-param { void* scanner }
%locations
%defines
%define parse.error verbose

%union {
    unsigned long long num;
    char* text;
    void* any;
    enum ContextIRType type;
}

%{
extern int yylex(YYSTYPE* yylval, YYLTYPE* yylloc, void* yyscanner);
extern const char* yyget_text(void* yyscanner);
void yyerror(YYLTYPE* yylloc, struct IRContext* context, const char* reason)
{
    contextYYError(yyget_lineno(context->m_scanner), yyget_column(context->m_scanner), context, reason, yyget_text(context->m_scanner));
}
#define scanner context->m_scanner
%}

%token NEWLINE ERR COMMA
%token SEPARATOR EQUAL
%token CHECKSTATE CHECKEQ CHECKMEMORY
%token LEFT_BRACKET RIGHT_BRACKET MEMORY
%token PLUS MINUS MULTIPLE DIVIDE

%token IRST_PUT IRST_EXIT IRST_STORE
%token IRST_STOREG IRST_LOADG
%token IREXP_CONST IREXP_RDTMP IREXP_LOAD
%token IREXP_GET


%token IRTY_I64 IRTY_I32 IRTY_I16 IRTY_I8 IRTY_I1

%token <num> INTNUM
%token <text> REGISTER_NAME IDENTIFIER

%type <any> expression readtmp_expression
%type <num> numberic_expression
%type <type> type_modifier

%left PLUS MINUS
%left MULTIPLE DIVIDE

%start input
%%

input:
  %empty
| register_init_list SEPARATOR statement_list SEPARATOR check_statment_list
;

register_init_list:
    %empty
| register_init_statment_line
| NEWLINE
| register_init_list register_init_statment_line
;

register_init_statment_line:
register_init_statment NEWLINE
;

register_init_statment:
    REGISTER_NAME EQUAL numberic_expression {
        contextSawRegisterInit(context, $1, $3);
        free($1);
    }
    | REGISTER_NAME EQUAL MEMORY LEFT_BRACKET numberic_expression COMMA numberic_expression RIGHT_BRACKET {
        contextSawRegisterInitMemory(context, $1, $5, $7);
        free($1);
    }
    | IDENTIFIER {
        contextSawInitOption(context, $1);
        free($1);
    }
;


statement_list:
  NEWLINE
| statement_line
| statement_list statement_line
;

statement_line:
statement NEWLINE
;

statement
    : IRST_EXIT numberic_expression {
        contextSawIRExit(context, $2);
    }
    | IDENTIFIER EQUAL expression {
        contextSawIRWr(context, $1, $3);
        free($1);
    }
    | IRST_PUT LEFT_BRACKET numberic_expression COMMA expression RIGHT_BRACKET {
        contextSawIRPut(context, $3, $5);
    }
    | IRST_STORE LEFT_BRACKET expression COMMA expression RIGHT_BRACKET {
        contextSawIRStore(context, $3, $5);
    }
    | IDENTIFIER EQUAL IRST_LOADG LEFT_BRACKET expression COMMA expression COMMA expression RIGHT_BRACKET {
        contextSawIRLoadG(context, $1, $5, $7, $9);
        free($1);
    }
    | IRST_STOREG LEFT_BRACKET expression COMMA expression COMMA expression RIGHT_BRACKET {
        contextSawIRStoreG(context, $3, $5, $7);
    }
    ;

expression
    : IREXP_CONST LEFT_BRACKET numberic_expression RIGHT_BRACKET {
        $$ = contextNewConstExpr(context, $3, CONTEXTIR_DEFAULT);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | IREXP_CONST LEFT_BRACKET numberic_expression COMMA type_modifier RIGHT_BRACKET {
        $$ = contextNewConstExpr(context, $3, $5);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | readtmp_expression
    | IREXP_LOAD LEFT_BRACKET expression RIGHT_BRACKET {
        $$ = contextNewLoadExpr(context, $3, CONTEXTIR_DEFAULT);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | IREXP_LOAD LEFT_BRACKET expression COMMA type_modifier RIGHT_BRACKET {
        $$ = contextNewLoadExpr(context, $3, $5);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | IREXP_GET LEFT_BRACKET REGISTER_NAME RIGHT_BRACKET {
        $$ = contextNewGetExpr(context, $3, CONTEXTIR_DEFAULT);
        free($3);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | IREXP_GET LEFT_BRACKET REGISTER_NAME COMMA type_modifier RIGHT_BRACKET {
        $$ = contextNewGetExpr(context, $3, $5);
        free($3);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    ;

readtmp_expression
    : IREXP_RDTMP LEFT_BRACKET IDENTIFIER RIGHT_BRACKET {
        $$ = contextNewRdTmpExpr(context, $3);
        free($3);
        if ($$ == NULL) {
            YYABORT;
        }
    }
    | IDENTIFIER {
        $$ = contextNewRdTmpExpr(context, $1);
        free($1);
        if ($$ == NULL) {
            YYABORT;
        }
    }
;

type_modifier
    : IRTY_I1 {
        $$ = CONTEXTIR_I1;
    }
    | IRTY_I8 {
        $$ = CONTEXTIR_I8;
    }
    | IRTY_I16 {
        $$ = CONTEXTIR_I16;
    }
    | IRTY_I32 {
        $$ = CONTEXTIR_I32;
    }
    | IRTY_I64 {
        $$ = CONTEXTIR_I64;
    }
    ;


numberic_expression
    : INTNUM {
        $$ = $1;
    }
    | numberic_expression PLUS numberic_expression {
        $$ = $1 + $3;
    }
    | numberic_expression MINUS numberic_expression {
        $$ = $1 - $3;
    }
    | numberic_expression MULTIPLE numberic_expression {
        $$ = $1 * $3;
    }
    | numberic_expression DIVIDE numberic_expression {
        $$ = $1 / $3;
    }
    | LEFT_BRACKET numberic_expression RIGHT_BRACKET {
        $$ = $2;
    }
    ;

check_statment_list:
    NEWLINE
| check_statment_line
| check_statment_list check_statment_line
;

check_statment_line:
check_statment NEWLINE
;

check_statment:
    CHECKEQ REGISTER_NAME numberic_expression {
        contextSawCheckRegisterConst(context, $2, $3);
        free($2);
    }
|   CHECKEQ REGISTER_NAME REGISTER_NAME {
        contextSawCheckRegister(context, $2, $3);
        free($2);
        free($3);
    }
| CHECKSTATE numberic_expression {
        contextSawCheckState(context, $2);
    }
| CHECKMEMORY REGISTER_NAME numberic_expression {
    contextSawCheckMemory(context, $2, $3);
    free($2);
}
;
%%
