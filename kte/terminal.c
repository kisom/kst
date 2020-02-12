#include <assert.h>
#include <curses.h>
#include <time.h>

#include "defs.h"


 */
static int	cury = 0;

void
nextline()
{
	printf("\t%d", cury);
	wrefresh(editor.main);
	cury++;
}


void
terminal_refresh()
{
	wrefresh(editor.main);
	wrefresh(editor.status);

	if ((time(NULL) - editor.msgtm) > KTE_MSG_TIME) {
		wrefresh(editor.message);
	}

	wmove(editor.main, cury, 0);
	if (cury > LINES-3) {
		cury = 0;
	}
}


/*
 * init follows the standard ncurses setup process: initialise the
 * screen, switch to non-buffered input, turn off local echo, and
 * allow capturing of special keys, which otherwise requires a
 * gnarly switch statement.
 *
 * Then, we need to set up the three windows: the editor main window,
 * a status line, and a message bar.
 */
void
terminal_init()
{
	initscr();
	cbreak();
	noecho();

	editor.main = newwin(LINES - 3, COLS-1, 0, 0);
	assert(editor.main != NULL);

	editor.status = newwin(1, COLS-1, LINES-3, 0);
	assert(editor.status != NULL);
	wattron(editor.status, A_REVERSE);	

	editor.message = newwin(1, COLS-1, LINES-2, 0);
	assert(editor.status != NULL);

	keypad(editor.main, TRUE);
	keypad(editor.message, TRUE);

	scrollok(editor.message, FALSE);
	scrollok(editor.status, FALSE);

	editor.msgtm = 0;
	wmove(editor.main, 0, 0);
}


void
terminal_deinit()
{
	endwin();
}


void
terminal_message(char *m, int l)
{
	if (l > COLS) {
		l = COLS;
	}

	for (int i = 0; i < l; i++) {
		waddch(editor.message, m[i]);
	}

	editor.msgtm = time(NULL);
	wrefresh(editor.message);
}


int
terminal_getch()
{
	return wgetch(editor.main);
}
