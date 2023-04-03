#include "pwmgr.h"

bool
hasnexttoken(struct input *input)
{
	return input->iToken < input->nTokens;
}

TOKEN *
peektoken(struct input *input, struct value *value)
{
	TOKEN *tok;

	if(input->iToken == input->nTokens)
		return NULL;
	tok = input->tokens + input->iToken;
	value->pos = tok->pos;
	value->word = input->buf + tok->pos;
	value->nWord = gettokenlen(input, input->iToken);
	if(tok->type == TSTRING)
	{
		value->word++;
		value->nWord -= 2;
	}
	return tok;
}

TOKEN *
nexttoken(struct input *input, struct value *value)
{
	TOKEN *tok;

	tok = peektoken(input, value);
	if(tok)
	{
		input->iToken++;
		return tok;
	}
	return NULL;
}

U32
gettokenlen(struct input *input, U32 token)
{
	TOKEN *tok;
	U32 l;
	bool e = false;
	char *str;

	tok = input->tokens + token;
	switch(tok->type)
	{
	case TWORD:
		str = input->buf + tok->pos;
		l = 0;
		while(isalnum(str[l]) || str[l] == '_')
			l++;
		break;
	case TSTRING:
		str = input->buf + tok->pos;
		l = 1;
		while(str[l] != '\"' || e)
		{
			e = !e && str[l] == '\\';
			l++;
		}
		l++;
		break;
	default:
		return 1;
	}
	return MIN(l, (U32) MAX_NAME);
}

int
tokenize(struct input *input)
{
	static const U32 map[0x100] = {
		[' '] = ESPACE,
		['\t'] = ESPACE,
		['\v'] = ESPACE,
		['\r'] = ESPACE,
		['\f'] = ESPACE,

		['\"'] = ZQUOTEBEGIN,
		['\\'] = ZQUOTEESCAPE,

		['a' ... 'z'] = TWORD,
		['A' ... 'Z'] = TWORD,
		['0' ... '9'] = TNUMBER,
		['_'] = TWORD,
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
	int errCode = OK;
	char *buf;
	TOKEN *tokens = NULL;
	U32 nTokens = 0, capTokens = 0;
	U32 last = 0;

	input->iToken = 0;
	tokens = input->tokens;
	capTokens = input->capTokens;
	for(buf = input->buf; *buf; buf++)
	{
		U32 m;

		m = map[(int) (unsigned char) *buf];
		switch(last)
		{
		case ZQUOTEBEGIN:
			if(m == ZQUOTEBEGIN)
			{
				tokens[nTokens - 1].type = TSTRING;
				last = TSTRING;
			}
			else if(m == ZQUOTEESCAPE)
			{
				last = ZQUOTEESCAPE;
			}
			goto fusion;
		case ZQUOTEESCAPE:
			if(m != ZQUOTEBEGIN)
			{
				nTokens--;
				input->errPos = buf - input->buf;
				errCode = ERR;
				goto end;
			}
			last = ZQUOTEBEGIN;
			goto fusion;
		case TWORD:
			if(m == TWORD || m == TNUMBER)
			{
				if(buf - input->buf - tokens[nTokens - 1].pos >= MAX_NAME)
				{
					input->errPos = buf - input->buf;
					errCode = ERR;
					goto end;
				}
				goto fusion;
			}
			break;
		case TNUMBER:
			if(m == TNUMBER)
				goto fusion;
			break;
		}
		if(!m) // unrecognized character
		{
			input->errPos = buf - input->buf;
			errCode = ERR;
			goto end;
		}
		if(m > EMAX)
		{
			if(nTokens == capTokens)
			{
				capTokens = 1 + capTokens * 2;
				tokens = realloc(tokens, sizeof(*tokens) * capTokens);
			}
			tokens[nTokens++] = (TOKEN) { m, buf - input->buf };
		}
		last = m;
	fusion:;
	}
	if(nTokens && tokens[nTokens - 1].type < ZMAX)
	{
		input->errPos = tokens[--nTokens].pos;
		errCode = ERR;
	}
end:
	input->tokens = tokens;
	input->nTokens = nTokens;
	input->capTokens = capTokens;
	return errCode;
}

