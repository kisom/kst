#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <wctype.h>


struct termios orig_termios;


void
disableRawMode()
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


void
enableRawMode()
{
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);

	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);

	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int
main()
{
	enableRawMode();

	wchar_t c;
	while (read(STDIN_FILENO, &c, sizeof(c)) > 0 && c != 'q') {
		if (iswcntrl(c)) {
			printf("$%02x\n", c);
		} else {
			printf("$%02x ('%lc')\n", c, c);
		}
	}

	perror("read");
	return 0;
}

