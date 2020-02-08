#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "display.h"
#include "keys.h"
#include "terminal.h"
#include "util.h"


void
loop()
{
	while (1) {
		process_keypress();
	}
}


int
main()
{
	setup_terminal();

	display_clear();
	loop();

	return 0;
}
