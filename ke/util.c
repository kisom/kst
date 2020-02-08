#include <stdio.h>
#include <stdlib.h>


void
die(const char *s)
{
	/*
	 * NOTE(kyle): this is a duplication of the code in display.c
	 * but I would like to be able to import these files from there.
	 */
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	perror(s);
	exit(1);
}
