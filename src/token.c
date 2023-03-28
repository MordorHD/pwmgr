#include "token.h"

INT getdecimal(TOKENIZER *tokenizer, U32 token)
{
	INT i = 0;
	char *str;
   
	str = tokenizer->line + tokenizer->tokens[token].pos;
	while(isdigit(*str))
	{
		i *= 10;
		i += *(str++) - '0';
	}
	return i;
}

INT
getbinary(TOKENIZER *tokenizer, U32 token)
{
	INT i = 0;
	char *str;
   
	str = tokenizer->line + tokenizer->tokens[token].pos + 2;
	while(*str == '0' || *str == '1')
	{
		i <<= 1;
		i += *(str++) - '0';
		while(isspace(*str))
			str++;
	}
	return i;
}

INT
getoctal(TOKENIZER *tokenizer, U32 token)
{
	INT i = 0;
	char *str;
   
	str = tokenizer->line + tokenizer->tokens[token].pos + 1;
	while(*str >= '0' && *str <= '7')
	{
		i <<= 3;
		i += *(str++) - '0';
		while(isspace(*str))
			str++;
	}
	return i;
}

INT
gethexadecimal(TOKENIZER *tokenizer, U32 token)
{
	INT i = 0;
	char *str;
   
	str = tokenizer->line + tokenizer->tokens[token].pos + 2;
	while(isxdigit(*str))
	{
		i <<= 4;
		i += isdigit(*str) ? *str - '0' : *str >= 'a' ? *str - 'a' + 10 : *str - 'A' + 10;
		str++;
		while(isspace(*str))
			str++;
	}
	return i;
}

FLOAT
getfloat(TOKENIZER *tokenizer, U32 token)
{
	char *str;
	FLOAT f = 0;
	FLOAT e = 1e-1;

	str = tokenizer->line + tokenizer->tokens[token].pos;
	while(isdigit(*str))
	{
		f *= 1e1;
		f += *(str++) - '0';
	}
	str++; // skip over .
	while(isdigit(*str))
	{
		f += (*str - '0') * e;
		e *= 1e-1;
		str++;
	}	
	return f;
}

U32
getwordlen(TOKENIZER *tokenizer, U32 token)
{
	U32 l = 0;
	char *str;

	str = tokenizer->line + tokenizer->tokens[token].pos;
	while(isalnum(str[l]) || str[l] == '_')
		l++;
	return l;
}

U32
getstringlen(TOKENIZER *tokenizer, U32 token)
{
	U32 l = 0;
	bool e = false;
	char *str;

	str = tokenizer->line + tokenizer->tokens[token].pos;
	str++;
	while(str[l] != '\"' || e)
	{
		if(str[l] == '\\')
			e = !e;
		else
			e = false;
		l++;
	}
	return l;
}

void
toktovalue(TOKENIZER *tokenizer, U32 token, struct value *value)
{
	TOKEN *tok;

	tok = tokenizer->tokens + token;
	value->pos = tok->pos;
	switch(tok->type)
	{
	case TWORD:
		value->word = tokenizer->line + tok->pos;
		value->nWord = getwordlen(tokenizer, token);
		break;
	case TSTRING:
		value->string = tokenizer->line + tok->pos + 1;
		value->nString = getstringlen(tokenizer, token);
		break;
	case TDECIMAL:
		value->i = getdecimal(tokenizer, token);
		break;
	case TBINARY:
		value->i = getbinary(tokenizer, token);
		break;
	case TOCTAL:
		value->i = getoctal(tokenizer, token);
		break;
	case THEXADECIMAL:
		value->i = gethexadecimal(tokenizer, token);
		break;
	default:
		attrset(COLOR_PAIR(3));
		printw("FATAL INTERNAL ERROR (tokenizer)");
		printf("\nPress any key to exit...");
		getch();
		endwin();
		abort();
	}
}

int
tokenize(TOKENIZER *tokenizer)
{
	// THE BEST AND MOST INTELEGENT TOKENIZER YOU'VE EVER SEEN
	static const U32 map[0x100] = {
		[' '] = ESPACE,
		['\t'] = ESPACE,
		['\v'] = ESPACE,
		['\r'] = ESPACE,
		['\f'] = ESPACE,

		['0'] = Z0,
		['1'] = Z1,
		['2' ... '7'] = Z27,
		['8' ... '9'] = Z89,
		['a' ... 'f'] = ZAF,
		['A' ... 'F'] = ZAF,
		['g' ... 'z'] = ZGZ,
		['G' ... 'Z'] = ZGZ,
		['_'] = ZGZ,

		// -Woverride-init
		['b'] = ZB,
		['B'] = ZB,
		['x'] = ZX,
		['X'] = ZX,

		['\"'] = ZQUOTEBEGIN,
		['\\'] = ZQUOTEESCAPE,

		[':'] = TCOLON,
		['.'] = TDOT,
		[','] = TCOMMA,
		['('] = TROPEN,
		[')'] = TRCLOSE,
		['['] = TCOPEN,
		[']'] = TCCLOSE,
		['%'] = TPERCENT,
		['!'] = TEXCLAM,
		['#'] = THASH,
		['@'] = TAT,
		['+'] = TPLUS,
		['-'] = TMINUS,
		['*'] = TMUL,
		['/'] = TDIV,
		['^'] = TRAISE,
		['&'] = TAND,
		['|'] = TOR,
		['<'] = TLSS,
		['>'] = TGTR,
		['='] = TAGN,
	};
	// connections of the tokens
	static const struct {
		U32 tok1, tok2;
		U32 resTok;
	} fusions[] = {
		// word
		{ ZB, ZB, TWORD },
		{ ZB, ZX, TWORD },
		{ ZB, ZAF, TWORD },
		{ ZB, ZGZ, TWORD },
		{ ZB, Z0, TWORD },
		{ ZB, Z1, TWORD },
		{ ZB, Z27, TWORD },
		{ ZB, Z89, TWORD },

		{ ZX, ZB, TWORD },
		{ ZX, ZX, TWORD },
		{ ZX, ZAF, TWORD },
		{ ZX, ZGZ, TWORD },
		{ ZX, Z0, TWORD },
		{ ZX, Z1, TWORD },
		{ ZX, Z27, TWORD },
		{ ZX, Z89, TWORD },

		{ ZAF, ZB, TWORD },
		{ ZAF, ZX, TWORD },
		{ ZAF, ZAF, TWORD },
		{ ZAF, ZGZ, TWORD },
		{ ZAF, Z0, TWORD },
		{ ZAF, Z1, TWORD },
		{ ZAF, Z27, TWORD },
		{ ZAF, Z89, TWORD },

		{ ZGZ, ZB, TWORD },
		{ ZGZ, ZX, TWORD },
		{ ZGZ, ZAF, TWORD },
		{ ZGZ, ZGZ, TWORD },
		{ ZGZ, Z0, TWORD },
		{ ZGZ, Z1, TWORD },
		{ ZGZ, Z27, TWORD },
		{ ZGZ, Z89, TWORD },

		{ TWORD, ZB, TWORD },
		{ TWORD, ZX, TWORD },
		{ TWORD, ZAF, TWORD },
		{ TWORD, ZGZ, TWORD },

		{ TWORD, Z0, TWORD },
		{ TWORD, Z1, TWORD },
		{ TWORD, Z27, TWORD },
		{ TWORD, Z89, TWORD },
		// string
		{ ZQUOTEBEGIN, ZQUOTEBEGIN, TSTRING },
		{ ZQUOTEESCAPE, ZQUOTEBEGIN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ESPACE, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, 0, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, Z0, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, Z1, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, Z27, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, Z89, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZAF, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZAF, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZGZ, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZGZ, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZGZ, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZB, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZB, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZX, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZX, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZQUOTEBEGIN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZQUOTEESCAPE, ZQUOTEESCAPE },
		{ ZQUOTEBEGIN, TCOLON, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TDOT, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TCOMMA, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TROPEN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TRCLOSE, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TCOPEN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TCCLOSE, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TPERCENT, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TEXCLAM, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, THASH, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TPLUS, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TMINUS, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TMUL, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TDIV, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TRAISE, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TAND, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TOR, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TLSS, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TGTR, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TAGN, ZQUOTEBEGIN },
		// numbers
		// float
		{ Z0, TDOT, TFLOAT },
		{ Z1, TDOT, TFLOAT },
		{ Z27, TDOT, TFLOAT },
		{ Z89, TDOT, TFLOAT },
		{ TDECIMAL, TDOT, TFLOAT },
		{ TDOT, Z0, TFLOAT },
		{ TDOT, Z1, TFLOAT },
		{ TDOT, Z27, TFLOAT },
		{ TDOT, Z89, TFLOAT },
		{ TFLOAT, Z0, TFLOAT },
		{ TFLOAT, Z1, TFLOAT },
		{ TFLOAT, Z27, TFLOAT },
		{ TFLOAT, Z89, TFLOAT },
		// binary
		{ Z0, ZB, Z0B }, // 0b 0B
		{ Z0B, Z0, TBINARY },
		{ Z0B, Z1, TBINARY },
		{ TBINARY, Z0, TBINARY },
		{ TBINARY, Z1, TBINARY },
		{ TBINARY, ESPACE, TBINARY },
		// decimal
		{ Z1, Z0, TDECIMAL },
		{ Z1, Z1, TDECIMAL },
		{ Z1, Z27, TDECIMAL },
		{ Z1, Z89, TDECIMAL },
		{ Z27, Z0, TDECIMAL },
		{ Z27, Z1, TDECIMAL },
		{ Z27, Z27, TDECIMAL },
		{ Z27, Z89, TDECIMAL },
		{ Z89, Z0, TDECIMAL },
		{ Z89, Z1, TDECIMAL },
		{ Z89, Z27, TDECIMAL },
		{ Z89, Z89, TDECIMAL },
		{ TDECIMAL, Z0, TDECIMAL },
		{ TDECIMAL, Z1, TDECIMAL },
		{ TDECIMAL, Z27, TDECIMAL },
		{ TDECIMAL, Z89, TDECIMAL },
		// octal
		{ Z0, Z0, TOCTAL },
		{ Z0, Z1, TOCTAL },
		{ Z0, Z27, TOCTAL },
		{ TOCTAL, Z0, TOCTAL },
		{ TOCTAL, Z1, TOCTAL },
		{ TOCTAL, Z27, TOCTAL },
		{ TOCTAL, ESPACE, TOCTAL },
		// hexadecimal
		{ Z0, ZX, Z0X }, // 0x 0X
		{ Z0X, Z0, THEXADECIMAL },
		{ Z0X, Z1, THEXADECIMAL },
		{ Z0X, Z27, THEXADECIMAL },
		{ Z0X, Z89, THEXADECIMAL },
		{ Z0X, ZAF, THEXADECIMAL },
		{ THEXADECIMAL, Z0, THEXADECIMAL },
		{ THEXADECIMAL, Z1, THEXADECIMAL },
		{ THEXADECIMAL, Z27, THEXADECIMAL },
		{ THEXADECIMAL, Z89, THEXADECIMAL },
		{ THEXADECIMAL, ZAF, THEXADECIMAL },
		{ THEXADECIMAL, ESPACE, THEXADECIMAL },
		// special characters
		{ TCOLON, TAGN, TCAGN }, // :=
		{ TAGN, TAGN, TEQU }, // ==
		{ TEXCLAM, TAGN, TNEQ }, // !=
		{ TGTR, TAGN, TGEQ }, // >=
		{ TLSS, TAGN, TLEQ }, // <=
		{ TGTR, TGTR, TRSH }, // >>
		{ TLSS, TLSS, TLSH }, // <<
		{ TAND, TAND, TLGAND }, // &&
		{ TOR, TOR, TLGOR }, // ||
	};
	// transformations
	static const U32 trans[ZMAX] = {
		[Z0] = TDECIMAL,
		[Z1] = TDECIMAL,
		[Z27] = TDECIMAL,
		[Z89] = TDECIMAL,
		[ZX] = TWORD,
		[ZB] = TWORD,
		[Z0B] = TERROR,
		[Z0X] = TERROR,
		[ZAF] = TWORD,
		[ZGZ] = TWORD,
		[ZQUOTEBEGIN] = TSTRING,
		[ZQUOTEESCAPE] = TERROR,
	};
	char *line;
	TOKEN *tokens = NULL;
	U32 nTokens = 0, capTokens = 0;
	U32 last = 0;

	for(line = tokenizer->line; *line; line++)
	{
		U32 m;
		
		m = map[(int) (unsigned char) *line];
		//printf("%d\n", cases[c].type);
		for(U32 f = 0; f < ARRLEN(fusions); f++)
			if(last == fusions[f].tok1 &&
				m == fusions[f].tok2)
			{
				if((last = fusions[f].resTok))
					tokens[nTokens - 1].type = fusions[f].resTok;
				goto made_fusion;
			}
		if(m > EMAX)
		{
			if(nTokens == capTokens)
			{
				capTokens = (capTokens + 1) * 2;
				tokens = realloc(tokens, sizeof(*tokens) * capTokens);
			}
			tokens[nTokens++] = (TOKEN) { m, line - tokenizer->line };
		}
		last = m;
	made_fusion:;
	}
	tokenizer->tokens = tokens;
	tokenizer->nTokens = nTokens;
	// transform tokens
	for(; nTokens; nTokens--, tokens++)
		if(tokens->type < ZMAX)
		{
			U32 to;

			to = trans[tokens->type];
			if(!to)
				return ERR;
			tokens->type = to;
		}
	return OK;
}

TOKEN *nexttoken(TOKENIZER *tokenizer)
{
	if(tokenizer->iToken == tokenizer->nTokens)
		return NULL;
	return tokenizer->tokens + tokenizer->iToken++;
}
