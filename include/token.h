#ifndef INCLUDED_TOKEN_H
#define INCLUDED_TOKEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <ncurses.h>
#define ARRLEN(a) (sizeof(a)/(sizeof*(a)))

typedef unsigned int U32;
typedef U32 INT;
typedef double FLOAT;

enum {
	TERROR,
	ESPACE,
	EMAX,
	Z0, Z1, Z27, Z89,
	ZAF, ZGZ, ZB, ZX,
	Z0B, Z0X,
	ZQUOTEBEGIN, ZQUOTEESCAPE,
	ZMAX,
	// literals
	TWORD, TSTRING, TDECIMAL, TOCTAL, TBINARY, THEXADECIMAL, TFLOAT,
	// special characters
	TCOLON, TDOT, TCOMMA,
	TROPEN, TRCLOSE, TCCLOSE, TCOPEN,
	TPERCENT, TEXCLAM, THASH, TAT,
	TPLUS, TMINUS, TMUL, TDIV, TRAISE,
	TAND, TOR, TLSS, TGTR, TAGN,
	TCAGN, TEQU, TNEQ, TGEQ, TLEQ, TRSH, TLSH,
	TLGAND, TLGOR,
};

struct value {
	U32 pos;
	union {
		struct {
			U32 nWord;
			char *word;
		};
		struct {
			U32 nString;
			char *string;
		};
		INT i;
		FLOAT f;
	};
};

typedef struct {
	U32 type;
	U32 pos;
} TOKEN;

typedef struct {
	char *line;
	TOKEN *tokens;
	U32 nTokens;
	U32 iToken;
} TOKENIZER;

int tokenize(TOKENIZER *tokenizer);
TOKEN *nexttoken(TOKENIZER *tokenizer);
INT getdecimal(TOKENIZER *tokenizer, U32 token);
INT getbinary(TOKENIZER *tokenizer, U32 token);
INT getotal(TOKENIZER *tokenizer, U32 token);
INT gethexadecimal(TOKENIZER *tokenizer, U32 token);
FLOAT getfloat(TOKENIZER *tokenizer, U32 token);
U32 getwordlen(TOKENIZER *tokenizer, U32 token);
void toktovalue(TOKENIZER *tokenizer, U32 token, struct value *value);

#endif
