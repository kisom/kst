/*
 * kyle's editor
 *
 * first version is a run-through of the kilo editor walkthrough as a
 * set of guiderails. I've made a lot of changes and did some things
 * differently. keep an eye for for kte, kyle's text editor - the
 * rewrite that will be coming out... when it comes out.
 *
 * https://viewsourcecode.org/snaptoken/kilo/
 */
#include <sys/ioctl.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


#define ESCSEQ		"\x1b["
#define	CTRL_KEY(key)	((key)&0x1f)
#define TAB_STOP	8
#define MSG_TIMEO	3

/*
 * define the keyboard input modes
 * normal: no special mode
 * kcommand: ^k commands
 * escape: what happens when you hit escape?
 */
#define	MODE_NORMAL	0
#define	MODE_KCOMMAND	1
#define	MODE_ESCAPE	2


#define	TAB_STOP	8


/*
 * Function declarations.
 */

/* append buffer */
struct abuf {
	char	*b;
	int	 len;
};
#define ABUF_INIT	{NULL, 0}

void	 ab_append(struct abuf *buf, const char *s, int len);
void	 ab_free(struct abuf *buf);


/* editor row */
struct erow {
	char	*line;
	char	*render;

	int	 size;
	int	 rsize;
};

char	 nibble_to_hex(char c);
int	 erow_render_to_cursor(struct erow *row, int cx);
int	 erow_cursor_to_render(struct erow *row, int rx);
void	 erow_update(struct erow *row);
void	 erow_insert(int at, char *s, int len);
void	 erow_free(struct erow *row);
void	 editor_set_status(const char *fmt, ...);

/* miscellaneous */
void		 die(const char *s);
int		 get_winsz(int *rows, int *cols);
void		 goto_line(void);
void		 delete_row(int at);
void		 row_append_row(struct erow *row, char *s, int len);
void		 row_insert_ch(struct erow *row, int at, int16_t c);
void		 row_delete_ch(struct erow *row, int at);
void		 insertch(int16_t c);
void		 deletech(void);
void		 open_file(const char *filename);
char		*rows_to_buffer(int *buflen);
int		 save_file(void);
uint16_t	 is_arrow_key(int16_t c);
int16_t		 get_keypress(void);
void		 display_refresh(void);
void		 editor_find_callback(char *query, int16_t c);
void		 editor_find(void);
char		*editor_prompt(char *, void (*cb)(char *, int16_t));
void		 editor_openfile(void);
void		 move_cursor(int16_t c);
void		 newline(void);
void		 process_kcommand(int16_t c);
void		 process_normal(int16_t c);
void		 process_escape(int16_t c);
int		 process_keypress(void);
void		 enable_termraw(void);
void		 display_clear(struct abuf *ab);
void		 disable_termraw(void);
void		 setup_terminal(void);
void		 draw_rows(struct abuf *ab);
char		 status_mode_char(void);
void		 draw_status_bar(struct abuf *ab);
void		 draw_message_line(struct abuf *ab);
void		 scroll(void);
void		 display_refresh(void);
void		 editor_set_status(const char *fmt, ...);
void		 loop(void);
void		 init_editor(void);
void		 process_normal(int16_t c);


enum KeyPress {
	TAB_KEY = 9,
	ESC_KEY = 27,
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PG_UP,
	PG_DN,
};


/*
 * editor is the global editor state; it should be broken out
 * to buffers and screen state, probably.
 */
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
} editor;


void
ab_append(struct abuf *buf, const char *s, int len)
{
	char	*nc = realloc(buf->b, buf->len + len);

	if (nc == NULL) {
		abort();
	}

	memcpy(&nc[buf->len], s, len);
	buf->b = nc;
	buf->len += len; /* DANGER: overflow */
}


void
ab_free(struct abuf *buf)
{
	free(buf->b);
	buf->b = NULL;
}

char
nibble_to_hex(char c)
{
	c &= 0xf;
	if (c < 10) {
		return c + 0x30;
	}
	return c + 0x41;
}


int
erow_render_to_cursor(struct erow *row, int cx)
{
	int	rx = 0;
	int	j;

	for (j = 0; j < cx; j++) {
		if (row->line[j] == '\t') {
			rx += (TAB_STOP-1) - (rx%TAB_STOP);
		} else if (row->line[j] < 0x20) {
			rx += 2;
		}
		rx++;
	}

	return rx;
}


int
erow_cursor_to_render(struct erow *row, int rx)
{
	int	cur_rx = 0;
	int	curx = 0;

	for (curx = 0; curx < row->size; curx++) {
		if (row->line[curx] == '\t') {
			cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
		} else if (row->line[curx] < 0x20) {
			cur_rx += 2;
		}
		cur_rx++;

		if (cur_rx > rx) {
			break;
		}
	}

	return curx;
}


void
erow_update(struct erow *row)
{
	int	i = 0, j;
	int	tabs = 0;
	int	ctrl = 0;

	/*
	 * TODO(kyle): I'm not thrilled with this double-render.
	 */
	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			tabs++;
		} else if (!isprint(row->line[j])) {
			ctrl++;
		}
	}

	if (row->rsize) {
		free(row->render);
		row->rsize = 0;
	}
	row->render = NULL;
	row->render = malloc(row->size + (tabs * (TAB_STOP-1)) + (ctrl * 3) + 1);
	assert(row->render != NULL);

	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			do {
				row->render[i++] = ' ';
			} while ((i % TAB_STOP) != 0);
		} else if (!isprint(row->line[j])) {
			row->render[i++] = '\\';
			row->render[i++] = nibble_to_hex(row->line[j] >> 4);
			row->render[i++] = nibble_to_hex(row->line[j] & 0x0f);
		} else {
			row->render[i++] = row->line[j];
		}
	}

	row->render[i] = '\0';
	row->rsize = i;
}


void
erow_insert(int at, char *s, int len)
{
	if (at < 0 || at > editor.nrows) {
		return;
	}

	editor.row = realloc(editor.row, sizeof(struct erow) * (editor.nrows + 1));
	assert(editor.row != NULL);

	if (at < editor.nrows) {
		memmove(&editor.row[at+1], &editor.row[at],
		    sizeof(struct erow) * (editor.nrows - at + 1));
	}

	editor.row[at].size = len;
	editor.row[at].line = malloc(len+1);
	memcpy(editor.row[at].line, s, len);
	editor.row[at].line[len] = '\0';

	editor.row[at].rsize = 0;
	editor.row[at].render = NULL;

	erow_update(&editor.row[at]);
	editor.nrows++;
}


void
erow_free(struct erow *row)
{
	free(row->render);
	free(row->line);
	row->render = NULL;
	row->line  = NULL;
}


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


/*
 * get_winsz uses the TIOCGWINSZ to get the window size.
 *
 * there's a fallback way to do this, too, that involves moving the
 * cursor down and to the left \x1b[999C\x1b[999B. I'm going to skip
 * on this for now because it's bloaty and this works on OpenBSD and
 * Linux, at least.
 */
int
get_winsz(int *rows, int *cols)
{
	struct winsize	ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
	    ws.ws_col == 0) {
		return -1;
	}

	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
}


void
goto_line()
{
	int	 lineno = 0;
	char	*query = editor_prompt("Line: %s", NULL);

	if (query == NULL) {
		return;
	}

	lineno = atoi(query);
	if (lineno < 1 || lineno >= editor.nrows) {
		editor_set_status("Line number must be between 1 and %d.", editor.nrows);
		return;
	}

	editor.cury = lineno - 1;
	editor.rowoffs = editor.cury - (editor.rows / 2);
}


void
delete_row(int at)
{
	if (at < 0 || at >= editor.nrows) {
		return;
	}

	erow_free(&editor.row[at]);
	memmove(&editor.row[at], &editor.row[at+1],
	    sizeof(struct erow) * (editor.nrows - at - 1));
	editor.nrows--;
	editor.dirty++;
}


void
row_append_row(struct erow *row, char *s, int len)
{
	row->line = realloc(row->line, row->size + len + 1);
	assert(row->line != NULL);
	memcpy(&row->line[row->size], s, len);
	row->size += len;
	row->line[row->size] = '\0';
	erow_update(row);
	editor.dirty++;
}


void
row_insert_ch(struct erow *row, int at, int16_t c)
{
	/*
	 * row_insert_ch just concerns itself with how to update a row.
	 */
	if ((at < 0) || (at > row->size)) {
		at = row->size;
	}
	assert(c > 0);

	row->line = realloc(row->line, row->size+2);
	assert(row->line != NULL);
	memmove(&row->line[at+1], &row->line[at], row->size - at + 1);
	row->size++;
	row->line[at] = c & 0xff;

	erow_update(row);
}


void
row_delete_ch(struct erow *row, int at)
{
	if (at < 0 || at >= row->size) {
		return;
	}
	memmove(&row->line[at], &row->line[at+1], row->size-at);
	row->size--;
	erow_update(row);
	editor.dirty++;
}


void
insertch(int16_t c)
{
	/*
	 * insert_ch doesn't need to worry about how to update a
	 * a row; it can just figure out where the cursor is
	 * at and what to do.
	 */
	if (editor.cury == editor.nrows) {
		erow_insert(editor.nrows, "", 0);
	}

	row_insert_ch(&editor.row[editor.cury], editor.curx, (char)(c & 0xff));
	editor.curx++;
	editor.dirty++;
}


void
deletech()
{
	struct erow	*row = NULL;

	if (editor.cury >= editor.nrows) {
		return;
	}

	if (editor.cury == 0 && editor.curx == 0) {
		return;
	}

	row = &editor.row[editor.cury];
	if (editor.curx > 0) {
		row_delete_ch(row, editor.curx - 1);
		editor.curx--;
	} else {
		editor.curx = editor.row[editor.cury - 1].size;
		row_append_row(&editor.row[editor.cury - 1], row->line,
		    row->size);
		delete_row(editor.cury);
		editor.cury--;
	}
}


void
open_file(const char *filename)
{
	char	*line = NULL;
	size_t	 linecap = 0;
	ssize_t	 linelen;
	FILE	*fp = NULL;

	free(editor.filename);
	editor.filename = strdup(filename);
	assert(editor.filename != NULL);

	editor.dirty = 0;
	if ((fp = fopen(filename, "r")) == NULL) {
		if (errno == ENOENT) {
			editor_set_status("[new file]");
			return;
		}
		die("fopen");
	}

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		if (linelen != -1) {
			while ((linelen > 0) && ((line[linelen-1] == '\r') ||
						 (line[linelen-1] == '\n'))) {
				linelen--;
			}

			erow_insert(editor.nrows, line, linelen);
		}
	}

	free(line);
	line = NULL;
	fclose(fp);
}


/*
 * convert our rows to a buffer; caller must free it.
 */
char *
rows_to_buffer(int *buflen)
{
	int	 len = 0;
	int	 j;
	char	*buf = NULL;
	char	*p = NULL;

	for (j = 0; j < editor.nrows; j++) {
		/* extra byte for newline */
		len += editor.row[j].size+1;
	}

	if (len == 0) {
		return NULL;
	}

	*buflen = len;
	buf = malloc(len);
	assert(buf != NULL);
	p = buf;

	for (j = 0; j < editor.nrows; j++) {
		memcpy(p, editor.row[j].line, editor.row[j].size);
		p += editor.row[j].size;
		*p++ = '\n';
	}

	return buf;
}


int
save_file()
{
	int	 fd = -1;
	int	 len;
	int	 status = 1; /* will be used as exit code */
	char	*buf;

	if (!editor.dirty) {
		editor_set_status("No changes to save.");
		return 0;
	}

	if (editor.filename == NULL) {
		editor.filename = editor_prompt("Filename: %s", NULL);
		if (editor.filename == NULL) {
			editor_set_status("Save aborted.");
			return 0;
		}
	}

	buf = rows_to_buffer(&len);
	if ((fd = open(editor.filename, O_RDWR|O_CREAT, 0644)) == -1) {
		goto save_exit;
	}

	if (-1 == ftruncate(fd, len)) {
		goto save_exit;
	}

	if (len == 0) {
		status = 0;
		goto save_exit;
	}

	if ((ssize_t)len != write(fd, buf, len)) {
		goto save_exit;
	}

	status = 0;

save_exit:
	if (fd)		close(fd);
	if (buf) {
		free(buf);
		buf = NULL;
	}

	if (status != 0) {
		buf = strerror(errno);
		editor_set_status("Error writing %s: %s", editor.filename,
		    buf);
	} else {
		editor_set_status("Wrote %d bytes to %s.", len,
		    editor.filename);
		editor.dirty = 0;
	}

	return status;
}


uint16_t
is_arrow_key(int16_t c)
{
	switch (c) {
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
	case ARROW_UP:
	case CTRL_KEY('p'):
	case CTRL_KEY('n'):
	case CTRL_KEY('f'):
	case CTRL_KEY('b'):
	case CTRL_KEY('a'):
	case CTRL_KEY('e'):
	case END_KEY:
	case HOME_KEY:
	case PG_DN:
	case PG_UP:
		return 1;
	};

	return 0;
}


int16_t
get_keypress()
{
	char	seq[3];
	char	c = -1;

	if (read(STDIN_FILENO, &c, 1) == -1) {
		die("get_keypress:read");
	}

	if (c == 0x1b) {
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return c;
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return c;

		if (seq[0] == '[') {
			if (seq[1] < 'A') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1)
					 return c;
				if (seq[2] == '~') {
					switch (seq[1]) {
					case '1': return HOME_KEY;
					case '3': return DEL_KEY;
					case '4': return END_KEY;
					case '5': return PG_UP;
					case '6': return PG_DN;
					case '7': return HOME_KEY;
					case '8': return END_KEY;
					}
				}
			} else {
				switch (seq[1]) {
				case 'A': return ARROW_UP;
				case 'B': return ARROW_DOWN;
				case 'C': return ARROW_RIGHT;
				case 'D': return ARROW_LEFT;
				case 'F': return END_KEY;
				case 'H': return HOME_KEY;

				default:
					/* nada */ ;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
			case 'F': return END_KEY;
			case 'H': return HOME_KEY;
			}
		}

		return 0x1b;
	}

	return c;
}


char *
editor_prompt(char *prompt, void (*cb)(char *, int16_t))
{
	size_t	 bufsz = 128;
	char	*buf = malloc(bufsz);
	size_t	 buflen = 0;
	int16_t	 c;

	buf[0] = '\0';

	while (1) {
		editor_set_status(prompt, buf);
		display_refresh();
		while ((c = get_keypress()) <= 0) ;
		if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
			if (buflen != 0) {
				buf[--buflen] = '\0';
			}
		} else if (c == ESC_KEY || c == CTRL_KEY('g')) {
			editor_set_status("");
			if (cb) {
				cb(buf, c);
			}
			free(buf);
			return NULL;
		} else if (c == '\r') {
			if (buflen != 0) {
				editor_set_status("");
				if (cb) {
					cb(buf, c);
				}
				return buf;
			}
		} else if (!iscntrl(c) && c < 128) {
			if (buflen == bufsz - 1) {
				bufsz *= 2;
				buf = realloc(buf, bufsz);
				assert(buf != NULL);
			}

			buf[buflen++] = c;
			buf[buflen] = '\0';
		}

		if (cb) {
			cb(buf, c);
		}
	}
}


void
editor_find_callback(char *query, int16_t c)
{
	static int	 lmatch = -1;
	static int	 dir = 1;
	int	 	 i, current;
	char		*match;
	struct erow	*row;

	if (c == '\r' || c == ESC_KEY) {
		/* reset search */
		lmatch = -1;
		dir = 1;
		return;
	} else if (c == ARROW_RIGHT || c == ARROW_DOWN || c == CTRL_KEY('s')) {
		dir = 1;
	} else if (c == ARROW_LEFT || c == ARROW_UP) {
		dir = -1;
	} else {
		lmatch = -1;
		dir = 1;
	}

	if (lmatch == -1) {
		dir = 1;
	}
	current = lmatch;

	for (i = 0; i < editor.nrows; i++) {
		current += dir;
		if (current == -1) {
			current = editor.nrows - 1;
		} else if (current == editor.nrows) {
			current = 0;
		}

		row = &editor.row[current];
		match = strstr(row->render, query);
		if (match) {
			lmatch = current;
			editor.cury = current;
			editor.curx = erow_render_to_cursor(row,
			    match - row->render);
			if (editor.curx > row->size) {
				editor.curx = row->size;
			}
			/*
			 * after this, scroll will put the match line at
			 * the top of the screen.
			 */
			editor.rowoffs = editor.nrows;
			break;
		}
	}

	display_refresh();
}


void
editor_find()
{
	char	*query;
	int	 scx = editor.curx;
	int	 scy = editor.cury;
	int	 sco = editor.coloffs;
	int	 sro = editor.rowoffs;

	query = editor_prompt("Search (ESC to cancel): %s",
	    editor_find_callback);
	if (query) {
		free(query);
	} else {
		editor.curx = scx;
		editor.cury = scy;
		editor.coloffs = sco;
		editor.rowoffs = sro;
	}

	display_refresh();
}


void
editor_openfile()
{
	char	*filename;

	/* TODO(kyle): combine with dirutils for tab-completion */
	filename = editor_prompt("Load file: %s", NULL);
	if (filename == NULL) {
		return;
	}

	free(editor.row);
	init_editor();

	open_file(filename);
}


void
move_cursor(int16_t c)
{
	struct erow	*row;
	int	 reps;

	row = (editor.cury >= editor.nrows) ? NULL : &editor.row[editor.cury];

	switch (c) {
	case ARROW_UP:
	case CTRL_KEY('p'):
		if (editor.cury > 0) {
			editor.cury--;
		}
		break;
	case ARROW_DOWN:
	case CTRL_KEY('n'):
		if (editor.cury < editor.nrows) {
			editor.cury++;
		}
		break;
	case ARROW_RIGHT:
	case CTRL_KEY('f'):
		if (row && editor.curx < row->size) {
			editor.curx++;
		} else if (row && editor.curx == row->size) {
			editor.cury++;
			editor.curx = 0;
		}
		break;
	case ARROW_LEFT:
	case CTRL_KEY('b'):
		if (editor.curx > 0) {
			editor.curx--;
		} else if (editor.cury > 0) {
			editor.cury--;
			editor.curx = editor.row[editor.cury].size;
		}
		break;
	case PG_UP:
	case PG_DN: {
		if (c == PG_UP) {
			editor.cury = editor.rowoffs;
		} else if (c == PG_DN) {
			editor.cury = editor.rowoffs + editor.rows-1;
			if (editor.cury > editor.nrows) {
				editor.cury = editor.nrows;
			}
		}

		reps = editor.rows;
		while (--reps) {
			move_cursor(c == PG_UP ? ARROW_UP : ARROW_DOWN);
		}

		break;
	}

	case HOME_KEY:
	case CTRL_KEY('a'):
		editor.curx = 0;
		break;
	case END_KEY:
	case CTRL_KEY('e'):
		if (editor.nrows == 0) {
			break;
		}
		editor.curx = editor.row[editor.cury].size;
		break;
	default:
		break;
	}


	row = (editor.cury >= editor.nrows) ?
	      NULL : &editor.row[editor.cury];
	reps = row ? row->size : 0;
	if (editor.curx > reps) {
		editor.curx = reps;
	}
}


void
newline()
{
	struct erow	*row = NULL;

	if (editor.curx == 0) {
		erow_insert(editor.cury, "", 0);
	} else {
		row = &editor.row[editor.cury];
		erow_insert(editor.cury + 1, &row->line[editor.curx],
		    row->size - editor.curx);
		row = &editor.row[editor.cury];
		row->size = editor.curx;
		row->line[row->size] = '\0';
		erow_update(row);
	}

	editor.cury++;
	editor.curx = 0;
}


void
process_kcommand(int16_t c)
{
	switch (c) {
	case 'q':
	case CTRL_KEY('q'):
		if (editor.dirty && editor.dirtyex) {
			editor_set_status("File not saved - C-k q again to quit.");
			editor.dirtyex = 0;
			return;
		}
		exit(0);
	case CTRL_KEY('s'):
	case 's':
		save_file();
		break;
	case CTRL_KEY('w'):
	case 'w':
		exit(save_file());
	case 'd':
		while ((editor.row[editor.cury].size - editor.curx) > 0) {
			process_normal(DEL_KEY);
		}
		break;
	case 'g':
	case CTRL_KEY('g'):
		goto_line();
		break;
	case BACKSPACE:
		while (editor.curx > 0) {
			process_normal(BACKSPACE);
		}
		break;
	case CTRL_KEY('\\'):
		/* sometimes it's nice to dump core */
		disable_termraw();
		abort();
	case 'e':
	case CTRL_KEY('e'):
		editor_openfile();
		break;
	case 'f':
		editor_find();
		break;
	case 'm':
		if (system("make") != 0) {
			editor_set_status("process failed: %s", strerror(errno));
		} else {
			editor_set_status("make: ok");
		}
		break;
	}

	editor.dirtyex = 1;
	return;
}


void
process_normal(int16_t c)
{
	if (is_arrow_key(c)) {
		move_cursor(c);
		return;
	}

	switch (c) {
	case '\r':
		newline();
		break;
	case CTRL_KEY('k'):
		editor.mode = MODE_KCOMMAND;
		return;
	case BACKSPACE:
	case CTRL_KEY('h'):
	case CTRL_KEY('d'):
	case DEL_KEY:
		if (c == DEL_KEY || c == CTRL_KEY('d')) {
			move_cursor(ARROW_RIGHT);
		}

		deletech();
		break;
	case CTRL_KEY('l'):
		display_refresh();
		break;
	case CTRL_KEY('s'):
		editor_find();
		break;
	case ESC_KEY:
		editor.mode = MODE_ESCAPE;
		break;
	default:
		if (isprint(c) || c == TAB_KEY) {
			insertch(c);
		}
		break;
	}

	editor.dirtyex = 1;
}


void
process_escape(int16_t c)
{
	struct erow	*row = &editor.row[editor.cury];

	editor_set_status("hi");

	switch (c) {
	case '>':
		editor.cury = editor.nrows;
		editor.curx = 0;
		break;
	case '<':
		editor.cury = 0;
		editor.curx = 0;
		break;
	case BACKSPACE:
		if (isalnum(row->line[editor.curx])) {
			editor_set_status("is alnum");
			while (editor.curx > 0 && isalnum(row->line[editor.curx])) {
				process_normal(BACKSPACE);
			}
		} else {
			editor_set_status("not alnum");
			process_normal(BACKSPACE);
		}
		break;
	default:
		editor_set_status("unknown ESC key: %04x", c);
	}
}


int
process_keypress()
{
	int16_t	c = get_keypress();


	/* we didn't actually read a key */
	if (c <= 0) {
		return 0;
	}

	switch (editor.mode) {
	case MODE_KCOMMAND:
		process_kcommand(c);
		editor.mode = MODE_NORMAL;
		break;
	case MODE_NORMAL:
		process_normal(c);
		break;
	case MODE_ESCAPE:
		process_escape(c);
		editor.mode = MODE_NORMAL;
		break;
	default:
		editor_set_status("we're in the %d-D space now cap'n", editor.mode);
		editor.mode = MODE_NORMAL;
	}

	return 1;
}


/*
 * A text editor needs the terminal to be in raw mode; but the default
 * is to be in canonical (cooked) mode, which is a buffered input mode.
 */
void
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


void
display_clear(struct abuf *ab)
{
	if (ab == NULL) {
		write(STDOUT_FILENO, ESCSEQ "2J", 4);
		write(STDOUT_FILENO, ESCSEQ "H", 3);
	} else {
		ab_append(ab, ESCSEQ "2J", 4);
		ab_append(ab, ESCSEQ "H", 3);
	}
}


void
disable_termraw()
{
	display_clear(NULL);

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.entry_term) == -1) {
		die("couldn't disable terminal raw mode");
	}
}


void
setup_terminal()
{
	if (tcgetattr(STDIN_FILENO, &editor.entry_term) == -1) {
		die("can't snapshot terminal settings");
	}
	atexit(disable_termraw);
	enable_termraw();
}


void
draw_rows(struct abuf *ab)
{
	assert(editor.cols >= 0);

	char	buf[editor.cols];
	int	buflen, filerow, padding;
	int	y;

	for (y = 0; y < editor.rows; y++) {
		filerow = y + editor.rowoffs;
		if (filerow >= editor.nrows) {
			if ((editor.nrows == 0) && (y == editor.rows / 3)) {
				buflen = snprintf(buf, sizeof(buf),
					"ke v%s", KE_VERSION);
				padding = (editor.rows - buflen) / 2;

				if (padding) {
					ab_append(ab, "|", 1);
					padding--;
				}

				while (padding--) ab_append(ab, " ", 1);
				ab_append(ab, buf, buflen);
			} else {
				ab_append(ab, "|", 1);
			}
		} else {
			erow_update(&editor.row[filerow]);
			buflen = editor.row[filerow].rsize - editor.coloffs;
			if (buflen < 0) {
				buflen = 0;
			}

			if (buflen > editor.cols) {
				buflen = editor.cols;
			}
			ab_append(ab, editor.row[filerow].render+editor.coloffs,
				  buflen);
		}
		ab_append(ab, ESCSEQ "K", 3);
		ab_append(ab, "\r\n", 2);
	}
}


char
status_mode_char()
{
	switch (editor.mode) {
	case MODE_NORMAL:
		return 'N';
	case MODE_KCOMMAND:
		return 'K';
	case MODE_ESCAPE:
		return 'E';
	default:
		return '?';
	}
}


void
draw_status_bar(struct abuf *ab)
{
	char	status[editor.cols];
	char	rstatus[editor.cols];
	int	len, rlen;

	len = snprintf(status, sizeof(status), "%c%cke: %.20s - %d lines",
	    status_mode_char(),
	    editor.dirty ? '!' : '-',
	    editor.filename ? editor.filename : "[no file]",
	    editor.nrows);
	rlen = snprintf(rstatus, sizeof(rstatus), "L%d/%d C%d",
	    editor.cury+1, editor.nrows, editor.curx+1);

	ab_append(ab, ESCSEQ "7m", 4);
	ab_append(ab, status, len);
	while (len < editor.cols) {
		if ((editor.cols - len) == rlen) {
			ab_append(ab, rstatus, rlen);
			break;
		}
		ab_append(ab, " ", 1);
		len++;
	}
	ab_append(ab, ESCSEQ "m", 3);
	ab_append(ab, "\r\n", 2);
}


void
draw_message_line(struct abuf *ab)
{
	int	len = strlen(editor.msg);

	ab_append(ab, ESCSEQ "K", 3);
	if (len > editor.cols) {
		len = editor.cols;
	}

	if (len && ((time(NULL) - editor.msgtm) < MSG_TIMEO)) {
		ab_append(ab, editor.msg, len);
	}
}


void
scroll()
{
	editor.rx = 0;
	if (editor.cury < editor.nrows) {
		editor.rx = erow_render_to_cursor(
				&editor.row[editor.cury],
				editor.curx);
	}

	if (editor.cury < editor.rowoffs) {
		editor.rowoffs = editor.cury;
	}

	if (editor.cury >= editor.rowoffs + editor.rows) {
		editor.rowoffs = editor.cury - editor.rows + 1;
	}

	if (editor.rx < editor.coloffs) {
		editor.coloffs = editor.rx;
	}

	if (editor.rx >= editor.coloffs + editor.cols) {
		editor.coloffs = editor.rx - editor.cols + 1;
	}
}


void
display_refresh()
{
	char		buf[32];
	struct abuf	ab = ABUF_INIT;

	scroll();

	ab_append(&ab, ESCSEQ "?25l", 6);
	display_clear(&ab);

	draw_rows(&ab);
	draw_status_bar(&ab);
	draw_message_line(&ab);

	snprintf(buf, sizeof(buf), ESCSEQ "%d;%dH",
		 (editor.cury-editor.rowoffs)+1,
		 (editor.rx-editor.coloffs)+1);
	ab_append(&ab, buf, strnlen(buf, 32));
	/* ab_append(&ab, ESCSEQ "1;2H", 7); */
	ab_append(&ab, ESCSEQ "?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}


void
editor_set_status(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(editor.msg, sizeof(editor.msg), fmt, ap);
	va_end(ap);

	editor.msgtm = time(NULL);
}


void
loop()
{
	int	up = 1; /* update on the first runthrough */

	while (1) {
		if (up) display_refresh();

		/*
		 * ke should only refresh the display if it has received keyboard
		 * input; if it has, drain all the inputs. This is useful for
		 * handling pastes without massive screen flicker.
		 *
		 */
		if ((up = process_keypress()) != 0) {
			while (process_keypress()) ;
		}
	}
}


/*
 * init_editor should set up the global editor struct.
 */
void
init_editor()
{
	editor.cols = 0;
	editor.rows = 0;

	if (get_winsz(&editor.rows, &editor.cols) == -1) {
		die("can't get window size");
	}
	editor.rows--; /* status bar */
	editor.rows--; /* message line */

	editor.curx = editor.cury = 0;
	editor.rx = 0;

	editor.nrows = 0;
	editor.rowoffs = editor.coloffs = 0;
	editor.row = NULL;

	editor.msg[0] = '\0';
	editor.msgtm = 0;

	editor.dirty = 0;
}


int
main(int argc, char *argv[])
{
	setup_terminal();
	init_editor();

	if (argc > 1) {
		open_file(argv[1]);
	}

	editor_set_status("C-k q to exit / C-k d to dump core");

	display_clear(NULL);
	loop();

	return 0;
}
