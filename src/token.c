#include "pwmgr.h"

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

		['a' ... 'z'] = TWORD,
		['A' ... 'Z'] = TWORD,
		['0' ... '9'] = TWORD,
		['_'] = TWORD,

		['\"'] = ZQUOTEBEGIN,
		['\\'] = ZQUOTEESCAPE,

		[':'] = TCOLON,
		['.'] = TDOT,
		[','] = TCOMMA,
		['%'] = TPERCENT,
		['!'] = TEXCLAM,
		['?'] = TQUESTION,
		['#'] = THASH,
		['@'] = TAT,
		['+'] = TPLUS,
		['-'] = TMINUS,
		['='] = TEQU,
	};
	// connections of the tokens
	static const struct {
		U32 tok1, tok2;
		U32 resTok;
	} fusions[] = {
		// word
		{ TWORD, TWORD, TWORD },
		// string
		{ ZQUOTEBEGIN, ZQUOTEBEGIN, TSTRING },
		{ ZQUOTEESCAPE, ZQUOTEBEGIN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ESPACE, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, 0, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZQUOTEBEGIN, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, ZQUOTEESCAPE, ZQUOTEESCAPE },
		{ ZQUOTEBEGIN, TWORD, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TCOLON, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TDOT, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TCOMMA, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TPERCENT, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TEXCLAM, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TQUESTION, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, THASH, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TPLUS, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TMINUS, ZQUOTEBEGIN },
		{ ZQUOTEBEGIN, TEQU, ZQUOTEBEGIN },
	};
	// transformations
	static const U32 trans[ZMAX] = {
		[ZQUOTEBEGIN] = TSTRING,
		[ZQUOTEESCAPE] = TERROR,
	};
	char *line;
	TOKEN *tokens = NULL;
	U32 nTokens = 0, capTokens = 0;
	U32 last = 0;

	tokens = tokenizer->tokens;
	capTokens = tokenizer->capTokens;
	for(line = tokenizer->line; *line; line++)
	{
		U32 m;
		
		m = map[(int) (unsigned char) *line];
		for(U32 f = 0; f < ARRLEN(fusions); f++)
			if(last == fusions[f].tok1 &&
				m == fusions[f].tok2)
			{
				if((last = fusions[f].resTok))
					tokens[nTokens - 1].type = fusions[f].resTok;
				goto made_fusion;
			}
		if(!m) // unrecognized character
		{
			tokenizer->errPos = line - tokenizer->line;
			return ERR;
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
	tokenizer->capTokens = capTokens;
	tokenizer->iToken = 0;
	// transform tokens
	for(; nTokens; nTokens--, tokens++)
		if(tokens->type < ZMAX)
		{
			U32 to;

			to = trans[tokens->type];
			if(!to)
			{
				tokenizer->errPos = tokens->pos;
				return ERR;
			}
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
