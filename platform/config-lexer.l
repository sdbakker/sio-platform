%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *current_file;
int current_lineno;

static int str_to_number(char *s);
static int str_to_float(char *s);
static int str_to_string(char *s);
float strtof(const char *nptr, char **endptr);

#include "sios_config.h"
#include "config-parser.h"
%}

bool	[yYnN]
num	[+-]?[0-9]+
float	[+-]?[0-9]+\.[0-9]+
hex	[+-]?0x[0-9a-fA-F]+
keyword	[[:alpha:]][[:alpha:]0-9_-]+
string	\"[^"]*\"|\'[^']*\'

%s parameters

%%

{keyword} {
	int keyw = sios_config_get_keyword(yytext);
	if (keyw != -1) {
		return keyw;
	}
	yylval.str = yytext;
	return M_PARAM;	
}

{num} {
	return str_to_number(yytext);
}

{hex} {
	return str_to_number(yytext);
}

{float} {
	return str_to_float(yytext);
}

{bool} {
	char b = yytext[0];
	if (b == 'y' || b == 'Y')
		yylval.bool = 1;
	else
		yylval.bool = 0;
	return BOOL;
}

{string} {
	return str_to_string(yytext);
}

"{" {
	return '{';
}

"}" {
  	return '}';
}

"#".*$ {
	/* eat comments */ 
	current_lineno++;
}

\n {
	current_lineno++;
}

[[:blank:]]+  { ; }

. {
  err("Config", "format error at line %d: '%s'", current_lineno, yytext);
}

%%

static int str_to_number(char *s)
{
	char *tail;
	long val = strtol(s,&tail,0);
	if (s == tail) {
		fprintf(stderr,"%d: Error parsing number '%s'\n", current_lineno, s);
	}
	yylval.num = val;
	return NUMBER;
}

static int str_to_float(char *s)
{
	char *tail;
	float val = strtof(s,&tail);
	if (s == tail) {
		fprintf(stderr,"%d: Error parsing float '%s'\n", current_lineno, s);
	}
	yylval.fnum = val;
	return FLOAT;
}

static int str_to_string(char *s)
{
    int n = strlen(s);
    yylval.str = malloc(n-1);
    strncpy(yylval.str, s+1, n-2);
    yylval.str[n-1] = '\0';
    return STRING;
}

