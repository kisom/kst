/*
 * defs.h
 * common definitions for kte.
 */


#ifndef KTE_DEFS_H
#define KTE_DEFS_H


#include <sys/queue.h>
#include <ncurses.h>


#define	KTE_TAB_STOP	8
#define	KTE_MSG_TIME	5

#define MODE_NORMAL	0
#define MODE_KCMD	1


struct file_buffer {
	char	*filename;
	int	 nrows;

	/* where are we in the file? */
	int	 curx, cury;
};


static struct {
	WINDOW	*main;
	WINDOW	*status;
	WINDOW	*message;

	time_t	 msgtm;

	LIST_HEAD(listhead, file_buffer) listhead;
} editor;


/* terminal.c */
void	terminal_refresh();
void	terminal_init();
void	terminal_deinit();
void	terminal_message(char *s, int l);
int	terminal_getch();

/* input.c */
int	handle_keypress(int c);


#endif /* KTE_DEFS_H */
