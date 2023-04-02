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

// reads one input character from the out pad from given y and x position
int getoutch(int y, int x);

enum {
	VARTYPE_U32,
	VARTYPE_I32,
	VARTYPE_STRING,
	VARTYPE_MAX,
};

struct variable {
	U32 type;
	const char *name;
	void *value;
};

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
	U32 type;
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
bool hasnexttoken(struct input *input);
TOKEN *peektoken(struct input *input, struct value *value);
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

static const struct {
	const char *name;
	const char *description;
	U32 token;	
} dependencies[] = {
	{ "account", "name", TWORD },
	{ "property", "name", TWORD },
	{ "value", "string", TSTRING },
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
