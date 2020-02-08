#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "util.h"


#define ESCSEQ		"\x1b["


static struct termios	entry_term;


/*
 * A text editor needs the terminal to be in raw mode; but the default
 * is to be in canonical (cooked) mode, which is a buffered input mode.
 */

static void
enable_termraw()
{
	struct termios	raw;

	/* Read the current terminal parameters for standard input. */
	if (tcgetattr(STDIN_FILENO, &raw) == -1) {
		die("tcgetattr while enabling raw mode");
	}

	/*
	 * Put the terminal into raw mode.
	 */
	cfmakeraw(&raw);

	/*
	 * Set timeout for read(2).
	 *
	 * VMIN: what is the minimum number of bytes required for read
	 * to return?
	 *
	 * VTIME: max time before read(2) returns in hundreds of milli-
	 * seconds.
	 */
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	/*
	 * Now write the terminal parameters to the current terminal,
	 * after flushing any waiting input out.
	 */
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		die("tcsetattr while enabling raw mode");
	}
}


static void
disable_termraw()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &entry_term) == -1) {
		die("couldn't disable terminal raw mode");
	}
}


void
setup_terminal()
{
	if (tcgetattr(STDIN_FILENO, &entry_term) == -1) {
		die("can't snapshot terminal settings");
	}
	atexit(disable_termraw);
	enable_termraw();
}


void
display_clear()
{
	write(STDOUT_FILENO, ESCSEQ "2J", 4);
	write(STDOUT_FILENO, ESCSEQ "H", 3);
}
