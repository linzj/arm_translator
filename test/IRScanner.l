%{
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "IRContext.h"
#include "IRParser.h"
%}
%option bison-bridge bison-locations reentrant noyywrap
%option nounput noyy_top_state
%option never-interactive
%option warn nodefault yylineno
%option extra-type="struct IRContext*"

h               [0-9a-fA-F]
nonascii        [\200-\377]
unicode         \\{h}{1,6}[ \t\r\n\f]?
escape          {unicode}|\\[ -~\200-\377]
nmstart         [_a-zA-Z]|{nonascii}|{escape}
nmchar          [_a-zA-Z0-9-]|{nonascii}|{escape}
string1         \"([^\n\r\f\\"]|\\{nl}|{escape})*\"
string2         \'([^\n\r\f\\']|\\{nl}|{escape})*\'
intnum          (0x)?[[:xdigit:]]+


ident           -?{nmstart}{nmchar}*

IRST_EXIT EXIT
IRST_PUT PUT
IRST_STORE STORE
IRST_STOREG STOREG
IRST_LOADG  LOADG
IREXP_RDTMP RDTMP
IREXP_CONST CONST
IREXP_LOAD LOAD
IREXP_GET GET
CHECKEQ CheckEqual
CHECKSTATE CheckState
CHECKMEMORY CheckMemory
MEMORY Memory
REGISTER_NAME rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp|r8|r9|r10|r11|r12|r13|r14|r15|rip
EQUAL =
IRTY_I64 i64
IRTY_I32 i32
IRTY_I16 i16
IRTY_I8 i8
IRTY_I1 i1
LEFT_BRACKET \(
RIGHT_BRACKET \)
IDENTIFIER {ident}
INTNUM {intnum}
NEWLINE [\n\r\f]
SPACE [[:space:]]+
SEPARATOR \%\%
COMMA ,
PLUS \+
MINUS \-
MULTIPLE \*
DIVIDE \/
%%
{IRST_EXIT}           return IRST_EXIT;
{IRST_PUT}           return IRST_PUT;
{IRST_STORE}           return IRST_STORE;
{IRST_LOADG}           return IRST_LOADG;
{IRST_STOREG}           return IRST_STOREG;

{IREXP_CONST}           return IREXP_CONST;
{IREXP_RDTMP}           return IREXP_RDTMP;
{IREXP_LOAD}      return IREXP_LOAD;
{IREXP_GET}      return IREXP_GET;

{COMMA}           return COMMA;
{CHECKEQ}           return CHECKEQ;
{CHECKSTATE}        return CHECKSTATE;
{CHECKMEMORY}        return CHECKMEMORY;
{REGISTER_NAME}     %{
                        yylval->text = strdup(yytext);
                        return REGISTER_NAME;
                    %}
{IRTY_I64} return IRTY_I64;
{IRTY_I32} return IRTY_I32;
{IRTY_I16} return IRTY_I16;
{IRTY_I8}  return IRTY_I8;
{IRTY_I1} return IRTY_I1;

{EQUAL}             return EQUAL;
{LEFT_BRACKET}      return LEFT_BRACKET;
{RIGHT_BRACKET}     return RIGHT_BRACKET;
{PLUS}              return PLUS;
{MINUS}             return MINUS;
{MULTIPLE}          return MULTIPLE;
{DIVIDE}            return DIVIDE;
{MEMORY}            return MEMORY;
{NEWLINE}           return NEWLINE;
{SPACE}            %{
                   %}
{SEPARATOR}         return SEPARATOR;
{IDENTIFIER}        %{
                        yylval->text = strdup(yytext);
                        return IDENTIFIER;
                    %}
{INTNUM}            %{
                        yylval->num = strtoull(yytext, NULL, 0);
                        return INTNUM;
                    %}
.                   %{
                        fprintf(stderr, "error string is %s.\n", yytext);
                        return ERR;
                    %}
%%
