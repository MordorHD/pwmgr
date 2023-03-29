#ifndef INCLUDED_PWMGR_H
#define INCLUDED_PWMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <ncurses.h>

#define ARRLEN(a) (sizeof(a)/(sizeof*(a)))
#define MAX(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a < _b ? _b : _a; \
})
#define MIN(a, b) ({ \
	__auto_type _a = (a); \
	__auto_type _b = (b); \
	_a > _b ? _b : _a; \
})

typedef uint32_t U32;
typedef int32_t I32;

enum {
	TERROR,
	ESPACE,
	EMAX,
	ZQUOTEBEGIN, ZQUOTEESCAPE,
	ZMAX,
	// literals
	TWORD, TSTRING,
	// special characters
	TCOLON, TDOT, TCOMMA,
	TPERCENT, TEXCLAM, TQUESTION, THASH, TAT,
	TPLUS, TMINUS, TEQU,
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
	};
};

typedef struct {
	U32 type;
	U32 pos;
} TOKEN;

typedef struct {
	char *line;
	TOKEN *tokens;
	U32 nTokens, capTokens;
	U32 iToken;
	U32 errPos;
} TOKENIZER;

int tokenize(TOKENIZER *tokenizer);
TOKEN *nexttoken(TOKENIZER *tokenizer);
void toktovalue(TOKENIZER *tokenizer, U32 token, struct value *value);

struct input {
	char buf[0x1000];
	U32 nBuf;
	U32 nextHistory;
	char history[0x8000];
	TOKENIZER tokenizer;
};

int getinput(struct input *input, bool isUtf8);

#endif
