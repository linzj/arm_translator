%{
#include <stdlib.h>
#include <stdio.h>
#include "IRContext.h"
%}
%pure-parser
%parse-param {struct IRContext* context}
%lex-param { void* scanner }
%locations
%define parse.error verbose

%union {
    unsigned long long num;
    char* text;
    double floatpoint;
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
%token CHECKEQFLOAT CHECKEQDOUBLE
%token <floatpoint> FLOATCONST


%token <num> INTNUM
%token <text> REGISTER_NAME IDENTIFIER

%type <num> numberic_expression

%left PLUS MINUS
%left MULTIPLE DIVIDE

%start input
%%

input:
  %empty
| register_init_list SEPARATOR check_statment_list
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
| CHECKEQFLOAT REGISTER_NAME FLOATCONST {
    contextSawCheckRegisterFloatConst(context, $2, $3);
    free($2);
}
;
%%
