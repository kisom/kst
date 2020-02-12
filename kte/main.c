/*
 * kyle's text editor
 *
 * this is the v2 from-scratch rewrite
 */


#include <curses.h>
#include <stdlib.h>

#include "defs.h"


void
deinit()
{
	endwin();
}


int
main()
{
	int	c, up = 1;

	terminal_init();
	terminal_message("welcome to KTE", 14);
	terminal_refresh();
	
	atexit(terminal_deinit);

	do {
		if (up)	{
			terminal_refresh();
		}
		c = terminal_getch();
		up = handle_keypress(c);
	} while (1);

	return 0;
}
