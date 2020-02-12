#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "defs.h"


static char
byte_to_hihex(char c)
{
	c = (c >> 4) & 0xf;

	if (c < 10) {
		return c + 0x30;
	}
	return c + 0x41;
}


static char
byte_to_lohex(char c)
{
	c &= 0x0f;

	if (c < 10) {
		return c + 0x30;
	}
	return c + 0x41;
}


int
erow_render_to_cursor(struct erow *row, int cx)
{
	int	rx = 0;
	int	j;

	for (j = 0; j < cx; j++) {
		if (row->line[j] == '\t') {
			rx += (TAB_STOP-1) - (rx%TAB_STOP);
		}
		rx++;
	}

	return rx;
}


int
erow_cursor_to_render(struct erow *row, int rx)
{
	int	cur_rx = 0;
	int	curx = 0;

	for (curx = 0; curx < row->size; curx++) {
		if (row->line[curx] == '\t') {
			cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
		}
		cur_rx++;

		if (cur_rx > rx) {
			break;
		}
	}

	return curx;
}


void
erow_update(struct erow *row)
{
	int	i = 0, j;
	int	tabs = 0;
	int	ctrl = 0;

	/*
	 * TODO(kyle): I'm not thrilled with this double-render.
	 */
	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			tabs++;
		} else if (!isprint(row->line[j])) {
			ctrl++;
		}
	}

	free(row->render);
	row->render = NULL;
	row->render = malloc(row->size + (tabs * (TAB_STOP-1)) + (ctrl * 3) + 1);
	assert(row->render != NULL);

	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			do {
				row->render[i++] = ' ';
			} while ((i % TAB_STOP) != 0);
		} else if (!isprint(row->line[j])) {
			row->render[i++] = '\\';
			row->render[i++] = byte_to_hihex(row->line[j]);
			row->render[i++] = byte_to_lohex(row->line[j]);
		} else {
			row->render[i++] = row->line[j];
		}
	}

	row->render[i] = '\0';
	row->rsize = i;
}


void
erow_insert(int at, char *s, int len)
{
	if (at < 0 || at > editor->nrows) {
		return;
	}

	editor->row = realloc(editor->row, sizeof(struct erow) * (editor->nrows + 1));
	assert(editor->row != NULL);

	if (at < editor->nrows) {
		printf("%d, %d\n", at, editor->nrows);
		memmove(&editor->row[at+1], &editor->row[at],
		    sizeof(struct erow) * (editor->nrows - at + 1));
	}

	editor->row[at].size = len;
	editor->row[at].line = malloc(len+1);
	memcpy(editor->row[at].line, s, len);
	editor->row[at].line[len] = '\0';

	editor->row[at].rsize = 0;
	editor->row[at].render = NULL;

	erow_update(&editor->row[at]);
	editor->nrows++;
}


void
erow_free(struct erow *row)
{
	free(row->render);
	free(row->line);
	row->render = NULL;
	row->line  = NULL;
}
