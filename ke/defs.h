#ifndef KE_DEFS_H
#define KE_DEFS_H


#include <termios.h>


#define	TAB_STOP	8


/*
 * abuf is an append buffer.
 */
struct abuf {
	char	*b;
	int	 len;
};

#define ABUF_INIT	{NULL, 0}


/*
 * erow is an editor row
 */
struct erow {
	char	*line;
	char	*render;

	int	 size;
	int	 rsize;
};


struct editor_t {
	struct termios	 entry_term;
	int		 rows, cols;
	int		 curx, cury;
	int		 rx;
	int		 mode;
	int		 nrows;
	int		 rowoffs, coloffs;
	struct erow	*row;
	char		*filename;
	int		 dirty;
	int		 dirtyex;
	char		 msg[80];
	time_t		 msgtm;
} _editor;


static struct editor_t	*editor = &_editor;


/*
 * Function declarations.
 */

/* append buffer */
void	ab_append(struct abuf *buf, const char *s, int len);
void	ab_free(struct abuf *buf);


/* editor row */
int	erow_render_to_cursor(struct erow *row, int cx);
int	erow_cursor_to_render(struct erow *row, int rx);
void	erow_update(struct erow *row);
void	erow_insert(int at, char *s, int len);
void	erow_free(struct erow *row);



#endif /* KE_DEFS_H */
