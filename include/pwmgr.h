#ifndef INCLUDED_PWMGR_H
#define INCLUDED_PWMGR_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
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

typedef int8_t I8;
typedef uint8_t U8;
typedef int32_t I32;
typedef uint32_t U32;
typedef int64_t I64;
typedef uint64_t U64;

#define MAX_NAME 64

#define ATTR_DEFAULT (COLOR_PAIR(0))
#define ATTR_HIGHLIGHT (COLOR_PAIR(0) | A_BOLD)
#define ATTR_SPECIAL (COLOR_PAIR(1))
#define ATTR_ERROR (COLOR_PAIR(2))
#define ATTR_FATAL (COLOR_PAIR(3))
#define ATTR_LOG (COLOR_PAIR(4))
#define ATTR_ADD (COLOR_PAIR(6))
#define ATTR_SUB (COLOR_PAIR(7))

#define ATTR_SYNTAX_KEYWORD (COLOR_PAIR(0) | A_BOLD)
#define ATTR_SYNTAX_WORD (COLOR_PAIR(0))
#define ATTR_SYNTAX_STRING (COLOR_PAIR(5) | A_DIM)

extern WINDOW *out;
extern int iPage;

void setoutpage(int page);

struct variable {
	char *name;
	char *value;
};

extern U32 area;
extern U32 inputHeight;

void addvariable(const struct variable *var);
struct variable *getvariable(const char *name, U32 nName);

// backup entries
// [id][time][additional information]
// addition information can be:
// [account]
// [property][account]
enum {
	BACKUP_ENTRY_ADDACCOUNT,
	BACKUP_ENTRY_REMOVEACCOUNT,
	BACKUP_ENTRY_ADDPROPERTY,
	BACKUP_ENTRY_REMOVEPROPERTY,
};

struct value {
	U32 pos;
	U32 type;
	union {
		I64 number;
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

enum {
	TERROR,
	ESPACE,
	EMAX,
	ZQUOTEBEGIN, ZQUOTEESCAPE,
	ZMAX,
	// literals
	TWORD, TSTRING, TNUMBER,
	// special characters
	TCOLON, TDOT, TCOMMA,
	TPERCENT, TEXCLAM, TQUESTION, THASH, TAT,
	TPLUS, TMINUS, TEQU,
};

typedef struct {
	U32 type;
	U32 pos;
} TOKEN;

#define MAX_INPUT 0x1000

struct input {
	WINDOW *win;
	char *buf;
	U32 nBuf;
	U32 nextHistory;
	char history[0x8000];
	TOKEN *tokens;
	U32 nTokens, capTokens;
	U32 iToken;
	U32 errPos;
};

int getinput(struct input *input, bool isUtf8);
int tokenize(struct input *input);
bool hasnexttoken(struct input *input);
TOKEN *peektoken(struct input *input, struct value *value);
TOKEN *nexttoken(struct input *input, struct value *value);
U32 gettokenlen(struct input *input, U32 token);

static const struct {
	const char *name;
	const char *description;
	U32 token;	
} dependencies[] = {
	{ "account", "name", TWORD },
	{ "property", "name", TWORD },
	{ "value", "string|number", 0 },
	{ "set", "name", TWORD },
};
#define IS_EXEC_BRANCH(branch) (!(branch)->nSubnodes || (I32) (branch)->nSubnodes == -1)
struct branch {
	const char *name;
	const char *description;
	U32 nSubnodes;
	union {
		const struct branch *subnodes;
		void (*proc)(const struct branch *branch, struct value *values);
		void (*special)(const struct branch *branch, struct input *input);
	};
};

extern const struct branch *const root; // defined in src/branch.c

void printoptions(const struct branch *branch);
const struct branch *nextbranch(const struct branch *branch, struct input *input);

#endif
