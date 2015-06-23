%{
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "IRContext.h"
#include "TestParser.h"
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
intnum          -?(0x)?[[:xdigit:]]+
D           [0-9]
E           [Ee][+-]?{D}+

ident           -?{nmstart}{nmchar}*

CHECKEQ CheckEqual
CHECKSTATE CheckState
CHECKMEMORY CheckMemory
CHECKEQFLOAT CheckEqualFloat
CHECKEQDOUBLE CheckEqualDouble
MEMORY Memory
REGISTER_NAME r([0-9]|1[0-5])
EQUAL =
LEFT_BRACKET \(
RIGHT_BRACKET \)
LEFT_BRACE \{
RIGHT_BRACE \{
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
DOT \.
INT8 int8
INT16 int16
INT32 int32
INT64 int64
%%

{COMMA}           return COMMA;
{CHECKEQ}           return CHECKEQ;
{CHECKSTATE}        return CHECKSTATE;
{CHECKMEMORY}        return CHECKMEMORY;
{CHECKEQFLOAT}      return CHECKEQFLOAT;
{CHECKEQDOUBLE}      return CHECKEQDOUBLE;
{REGISTER_NAME}     %{
                        yylval->text = strdup(yytext);
                        return REGISTER_NAME;
                    %}

{EQUAL}             return EQUAL;
{LEFT_BRACKET}      return LEFT_BRACKET;
{RIGHT_BRACKET}     return RIGHT_BRACKET;
{DOT}               return DOT;
{INT8}              yylval->inttype = 8; return INTTYPE;
{INT16}              yylval->inttype = 16; return INTTYPE;
{INT32}              yylval->inttype = 32; return INTTYPE;
{INT64}              yylval->inttype = 64; return INTTYPE;
{LEFT_BRACE}        return LEFT_BRACE;
{RIGHT_BRACE}        return RIGHT_BRACE;
{PLUS}              return PLUS;
{MINUS}             return MINUS;
{MULTIPLE}          return MULTIPLE;
{DIVIDE}            return DIVIDE;
{MEMORY}            return MEMORY;
{NEWLINE}           return NEWLINE;
{SPACE}            %{
                   %}
{SEPARATOR}         return SEPARATOR;
{D}+{E}           %{
                        yylval->floatpoint = strtod(yytext, NULL);
                        return FLOATCONST;
                   %}
{D}+"."{D}*({E})? %{
                        yylval->floatpoint = strtod(yytext, NULL);
                        return FLOATCONST;
                   %}
"."{D}+({E})?     %{
                        yylval->floatpoint = strtod(yytext, NULL);
                        return FLOATCONST;
                   %}
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
