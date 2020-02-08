#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>


/* entry_term contains a snapshot of the termios settings at startup. */
struct termios	entry_term;


/*
 * A text editor needs the terminal to be in raw mode; but the default
 * is to be in canonical (cooked) mode, which is a buffered input mode.
 */

static void
enable_termraw()
{
	struct termios	raw;

	/* Read the current terminal parameters for standard input. */
	tcgetattr(STDIN_FILENO, &raw);

	/*
	 * Turn off the local ECHO mode, which we need to do in raw mode
	 * because what gets displayed is going to need extra control.
	 *
	 * NOTE(kyle): look into cfmakeraw, which will require
	 * snapshotting.
	 */
	raw.c_lflag &= ~(ECHO|ICANON);

	/*
	 * Now write the terminal parameters to the current terminal,
	 * after flushing any waiting input out.
	 */
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


static void
disable_termraw()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &entry_term);
}


int
main()
{
	char	c;

	/* prepare the terminal */
	tcgetattr(STDIN_FILENO, &entry_term);
	atexit(disable_termraw);
	enable_termraw();

	while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
		if (iscntrl(c)) {
			printf("%02x\n", c);
		} else {
			printf("%02x ('%c')\n", c, c);
		}
	}

	return 0;
}
