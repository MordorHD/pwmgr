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

#define ATTR_DEFAULT (COLOR_PAIR(0))
#define ATTR_HIGHLIGHT (COLOR_PAIR(0) | A_BOLD)
#define ATTR_SPECIAL (COLOR_PAIR(1))
#define ATTR_ERROR (COLOR_PAIR(2))
#define ATTR_FATAL (COLOR_PAIR(3))
#define ATTR_LOG (COLOR_PAIR(4))

#define ATTR_SYNTAX_KEYWORD (COLOR_PAIR(0) | A_BOLD)
#define ATTR_SYNTAX_WORD (COLOR_PAIR(0))
#define ATTR_SYNTAX_STRING (COLOR_PAIR(5) | A_DIM)

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

struct input;

int tokenize(struct input *input);
TOKEN *nexttoken(struct input *input, struct value *value);
U32 gettokenlen(struct input *input, U32 token);

struct input {
	char buf[0x1000];
	U32 nBuf;
	U32 nextHistory;
	char history[0x8000];
	TOKEN *tokens;
	U32 nTokens, capTokens;
	U32 iToken;
	U32 errPos;
};

int getinput(struct input *input, bool isUtf8);

#endif
