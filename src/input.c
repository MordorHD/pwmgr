#include "pwmgr.h"

void
renderinput(struct input *input, int i, int n, int x, int y)
{
	int cy, cx;

	// erase input
	for(int l = y; l < LINES - 1; l++)
	for(int c = 0; c < COLS; c++)
		mvaddch(l, c, ' ');
	for(int c = 0; c < COLS - 1; c++)
		mvaddch(LINES - 1, c, ' ');
	// redraw input in two steps (from beginning to cursor and then from cursor to end)
	mvaddnstr(y, x, input->buf, i);
	getyx(stdscr, cy, cx);
	addnstr(input->buf + i, input->nBuf - i);
	move(cy, cx);
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
		int cx, cy, ch;

		renderinput(input, iBuf, nBuf, x, y);
		getyx(stdscr, cy, cx);
		ch = getch();
		if(ch == '\n')
			break;
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
	if(!nBuf)
		return 0;
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
	return nBuf;
}
