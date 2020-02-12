/*
 * input.c: process key strokes and what not.
 */

#include <stdio.h>

#include "defs.h"


/*
 * given the key, maybe do something with it, and return whether this
 * means the screen should be refreshed.
 */
int
handle_keypress(int c)
{
	static int mode = MODE_NORMAL;

	if (mode == MODE_NORMAL) {
		printf("%02x\t", c);	
	}

	return 1;
}
