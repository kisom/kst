#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"
#include "util.h"


void
loop()
{
	char	c;
	int	command = 0;

	while (1) {
		c = '\0';
		/*
		 * Note that Cygwin, apparently, will treat a read time-
		 * out as an error with errno == EAGAIN. I don't really
		 * have any interest in supporting code for Cygwin.
		 */
		if ((read(STDIN_FILENO, &c, 1) == -1)) {
			die("failed to read from the terminal");
		}

		if (c == 0x00) {
			continue;
		}

		if (command && ((c == 'q') || (c == 0x11))) {
			break;
		}

		if (c == 0x0b) {
			if (!command) {
				command = 1;
				continue;
			}
		} else {
			command = 0;
		}

		if (iscntrl(c)) {
			printf("%02x\r\n", c);
		} else {
			printf("%02x ('%c')\r\n", c, c);
		}
	}
}


int
main()
{
	setup_terminal();
	loop();

	return 0;
}
