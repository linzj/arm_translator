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


ident           -?{nmstart}{nmchar}*

CHECKEQ CheckEqual
CHECKSTATE CheckState
CHECKMEMORY CheckMemory
MEMORY Memory
REGISTER_NAME r([0-9]|1[0-5])
EQUAL =
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

{COMMA}           return COMMA;
{CHECKEQ}           return CHECKEQ;
{CHECKSTATE}        return CHECKSTATE;
{CHECKMEMORY}        return CHECKMEMORY;
{REGISTER_NAME}     %{
                        yylval->text = strdup(yytext);
                        return REGISTER_NAME;
                    %}

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
