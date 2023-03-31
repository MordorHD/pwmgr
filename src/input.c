#include "pwmgr.h"

int
renderinput(struct input *input, U32 iBuf, U32 nBuf, int x, int y, int *pCurX, int *pCurY)
{
	int cy, cx;
	int err;
	TOKEN *last = NULL;
	U32 tokenLen;

	// measure cursor position
	mvaddnstr(y, x, input->buf, iBuf);
	getyx(stdscr, cy, cx);
	// erase input
	attrset(ATTR_DEFAULT);
	move(y, 0);
	for(int i = 0, n = (LINES - y - 1) * COLS + COLS - 1; i < n; i++)
		addch(' ');
	move(y, x);
	input->buf[nBuf] = 0;
	err = tokenize(input);
	for(U32 i = 0, nTokens = input->nTokens; i < nTokens; i++)
	{
		TOKEN *tok;

		tok = input->tokens + i;
		if(last)
		{
			attrset(ATTR_DEFAULT);
			addnstr(input->buf + last->pos + tokenLen, tok->pos - last->pos - tokenLen);
		}
		switch(tok->type)
		{
		case TWORD:
			attrset(ATTR_SYNTAX_WORD);
			break;
		case TSTRING:
			attrset(ATTR_SYNTAX_STRING);
			break;
		default:
			attrset(ATTR_SYNTAX_KEYWORD);
		}
		tokenLen = gettokenlen(input, i);
		addnstr(input->buf + tok->pos, tokenLen);
		last = tok;
	}
	if(err)
	{
		U32 pos;

		pos = input->errPos;
		if(last)
		{
			attrset(ATTR_DEFAULT);
			addnstr(input->buf + last->pos + tokenLen, pos - last->pos - tokenLen);
		}
		attrset(ATTR_ERROR);
		addnstr(input->buf + pos, nBuf - pos);
	}
	*pCurX = cx;
	*pCurY = cy;
	return err;
}

int
getinput(struct input *input, bool isUtf8)
{
	char *buf;
	char saveBuf[sizeof(input->buf)];
	U32 maxBuf;
	U32 iBuf, nBuf;
	int x, y;
	char *history;
	U32 maxHistory;
	U32 nextHistory;
	I32 overflowHistory;
	U32 curHistory;
	U32 lastHistory;
	int tokErr;

	input->nBuf = 0;
	buf = input->buf;
	maxBuf = sizeof(input->buf);
	iBuf = 0;
	nBuf = 0;
	history = input->history;
	maxHistory = sizeof(input->history);
	nextHistory = input->nextHistory;
	curHistory = nextHistory;

	getyx(stdscr, y, x);
	while(1)
	{
		int cx, cy, ex, ey, ch;

		tokErr = renderinput(input, iBuf, nBuf, x, y, &cx, &cy);
		getyx(stdscr, ey, ex);
		move(cy, cx);
		ch = getch();
		if(ch == '\n')
		{
			move(ey, ex);
			break;
		}
		if(ch >= 0x20 && ((ch <= 0xFF && isUtf8) || ch < 0x7F))
		{
			char bUtf8[12];
			U32 nUtf8 = 1;
			U32 headUtf8 = ch;

			bUtf8[0] = ch;
			if(headUtf8 & 0x80)
			{
				U32 mask;

				mask = 0x80 >> 1;
				while(headUtf8 & mask)
				{
					mask >>= 1;
					bUtf8[nUtf8++] = getch();
				}
			}
			if(nBuf + nUtf8 >= maxBuf)
				continue;
			memmove(buf + iBuf + nUtf8, buf + iBuf, nBuf - iBuf);
			memcpy(buf + iBuf, bUtf8, nUtf8);
			iBuf += nUtf8;
			nBuf += nUtf8;
			// check if scrolling will occur
			if(cy == LINES - 1 && cx == COLS - 1)
			{
				wscrl(stdscr, 1);
				y--;
			}
			// this is so the save buf can be updated at the next call of up
			curHistory = nextHistory;
			continue;
		}
		switch(ch)
		{
		case KEY_HOME:
			iBuf = 0;
			break;
		case KEY_END:
			iBuf = nBuf;
			break;
		case KEY_LEFT:
			if(iBuf)
			{
				iBuf--;
				while(iBuf && ((buf[iBuf] & 0xC0) == 0x80))
					iBuf--;
			}
			break;
		case KEY_RIGHT:
			if(iBuf < nBuf)
			{
				iBuf++;
				while(iBuf < nBuf && ((buf[iBuf] & 0xC0) == 0x80))
					iBuf++;
			}
			break;
		case KEY_BACKSPACE:
			if(iBuf)
			{
				U32 nRem = 1;

				iBuf--;
				while(iBuf && ((buf[iBuf] & 0xC0) == 0x80))
				{
					nRem++;
					iBuf--;
				}
				nBuf -= nRem;
				memmove(buf + iBuf, buf + iBuf + nRem, nBuf - iBuf);
			}
			break;
		case KEY_DC:
			if(iBuf != nBuf)
			{
				U32 nRem = 1;

				while(iBuf + nRem < nBuf && ((buf[iBuf + nRem] & 0xC0) == 0x80))
					nRem++;
				nBuf -= nRem;
				memmove(buf + iBuf, buf + iBuf + nRem, nBuf - iBuf);
			}
			break;
		case KEY_UP:
			if(curHistory)
			{
				if(curHistory == nextHistory)
				{
					memcpy(saveBuf, buf, nBuf);
					saveBuf[nBuf] = 0;
				}
				curHistory -= 2;
				while(curHistory && history[curHistory])
					curHistory--;
				if(curHistory)
					curHistory++;
				strcpy(buf, history + curHistory);
				iBuf = nBuf = strlen(buf);
			}
			break;
		case KEY_DOWN:
		{
			U32 l = 0;

			l = strlen(history + curHistory);
			if(l)
			{
				curHistory += l + 1;
				l = strlen(history + curHistory);
				if(l)
				{
					strcpy(buf, history + curHistory);
					iBuf = nBuf = l;
					break;
				}
			}
			iBuf = nBuf = strlen(saveBuf);
			memcpy(buf, saveBuf, nBuf);
			break;
		}
		}
	}
	input->nBuf = nBuf;
	buf[nBuf] = 0;
	if(!input->nTokens)
		return ERR;
	lastHistory = curHistory;
	if(curHistory)
	{
		lastHistory -= 2;
		while(lastHistory && history[lastHistory])
			lastHistory--;
		if(lastHistory)
			lastHistory++;
	}
	if(!curHistory || strcmp(history + lastHistory, buf))
	{
		overflowHistory = nextHistory - maxHistory + nBuf + 1 + 1; // +1 for null terminator and +1 for extra space needed by the next history cursor
		if(overflowHistory > 0)
		{
			const U32 l = overflowHistory + strlen(history + overflowHistory) + 1;
			nextHistory -= l;
			memmove(history, history + l, nextHistory);
		}
		memcpy(history + nextHistory, buf, nBuf + 1);
		nextHistory += nBuf + 1;
	}
	input->nextHistory = nextHistory;
	return tokErr;
}
