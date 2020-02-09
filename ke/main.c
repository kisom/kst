#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


#define	KE_VERSION	"0.0.1-pre"
#define ESCSEQ		"\x1b["
#define	CTRL_KEY(key)	((key)&0x1f)

/*
 * define the keyboard input modes
 * normal: no special mode
 * kcommand: ^k commands
 */
#define	MODE_NORMAL	0
#define	MODE_KCOMMAND	1
#define	MODE_INSERT	2


enum KeyPress {
	ARROW_UP = 1000,
	ARROW_DOWN,
	ARROW_LEFT,
	ARROW_RIGHT,
	DEL_KEY,
	END_KEY,
	HOME_KEY,
	PG_UP,
	PG_DN,
};


void	disable_termraw();

typedef struct erow {
	char	*line;
	int	 size;
} erow;

struct {
	struct termios	 entry_term;
	int		 rows, cols;
	int		 curx, cury;
	int		 mode;
	int		 nrows;
	erow		*row;
} editor;


/* append buffer */
struct abuf {
	char	*b;
	int	 len;
};
#define ABUF_INIT	{NULL, 0}


void
ab_append(struct abuf *buf, const char *s, int len)
{
	char	*nc = realloc(buf->b, buf->len + len);

	if (nc == NULL) {
		return;
	}

	memcpy(&nc[buf->len], s, len);
	buf->b = nc;
	buf->len += len; /* DANGER: overflow */
}


void
ab_free(struct abuf *buf)
{
	free(buf->b);
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
append_row(char *s, size_t len)
{
	int	at = editor.nrows;

	editor.row = realloc(editor.row, sizeof(erow) * (editor.nrows + 1));
	editor.row[at].size = len;
	editor.row[at].line = malloc(len+1);
	memcpy(editor.row[at].line, s, len);
	editor.row[at].line[len] = '\0';
	editor.nrows++;
}


void
open_file(const char *filename)
{
	char	*line = NULL;
	size_t	 linecap = 0;
	ssize_t	 linelen;
	FILE	*fp = fopen(filename, "r");
	if (!fp) {
		die("fopen");
	}

	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		if (linelen != -1) {
			while ((linelen > 0) && ((line[linelen-1] == '\r') ||
						 (line[linelen-1] == '\n'))) {
				linelen--;
			}

			append_row(line, linelen);
		}
	}

	free(line);
	fclose(fp);
}


uint16_t
is_arrow_key(int16_t c)
{
	switch (c) {
	case ARROW_UP:
	case ARROW_DOWN:
	case ARROW_LEFT:
	case ARROW_RIGHT:
	case CTRL_KEY('p'):
	case CTRL_KEY('n'):
	case CTRL_KEY('f'):
	case CTRL_KEY('b'):
	case PG_UP:
	case PG_DN:
	case HOME_KEY:
	case END_KEY:
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
					case '2': return END_KEY;
					case '3': return DEL_KEY;
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


static inline void
ymov(int dy)
{
	editor.cury += dy;
	if (editor.cury < 0) {
		editor.cury = 0;
	} else if (editor.cury > editor.rows) {
		/* 
		 * NOTE(kyle): this should be restrited to buffer lines,
		 * but we don't have a buffer yet.
		 */
		editor.cury = editor.rows;
	}
}


static inline void
xmov(int dx)
{
	editor.curx += dx;
	if (editor.curx < 0) {
		editor.curx = 0;
		ymov(-1);
	} else if (editor.curx > editor.cols) {
		if (editor.cury == editor.rows) {
			editor.curx = editor.cols;
		} else {
			editor.curx = 0;
		}
		ymov(1);
	}
}


void
move_cursor(int16_t c)
{
	int	reps;

	switch (c) {
	case ARROW_UP:
	case CTRL_KEY('p'):
		ymov(-1);
		break;
	case ARROW_DOWN:
	case CTRL_KEY('n'):
		ymov(1);
		break;
	case ARROW_RIGHT:
	case CTRL_KEY('f'):
		xmov(1);
		break;
	case ARROW_LEFT:
	case CTRL_KEY('b'):
		xmov(-1);
		break;
	case PG_UP:
	case PG_DN: {
		reps = editor.rows;
		while (--reps) {
			move_cursor(c == PG_UP ? ARROW_UP : ARROW_DOWN);
		}				
	}

	case HOME_KEY:
		editor.curx = 0;
		break;
	case END_KEY:
		editor.curx = editor.cols - 1;
		break;
	default:
		break;
	}
}


void
process_kcommand(int16_t c)
{
	switch (c) {
	case 'q':
	case CTRL_KEY('q'):
		exit(0);
	case 'd':
	case CTRL_KEY('\\'):
		/* sometimes it's nice to dump core */
		disable_termraw();
		abort();
	}

	editor.mode = MODE_NORMAL;
	return;
}


void
process_keypress()
{
	int16_t	c = get_keypress();

	/* we didn't actually read a key */
	if (c <= 0) {
		return;
	}

	switch (editor.mode) {
	case MODE_KCOMMAND:
		process_kcommand(c);
		break;
	case MODE_NORMAL:
		if (is_arrow_key(c)) {
			move_cursor(c);
		}
	}

	if (c == CTRL_KEY('k')) {
		editor.mode = MODE_KCOMMAND;
		return;
	}
}


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
	char	buf[editor.cols];
	int	buflen, padding;
	int	y;

	for (y = 0; y < editor.rows; y++) {
		if (y >= editor.nrows) {
			if ((editor.nrows == 0) && (y == editor.rows / 3)) {
				buflen = snprintf(buf, sizeof(buf),
					"ke k%s", KE_VERSION);
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
			buflen = editor.row[y].size;
			if (buflen > editor.rows) {
				buflen = editor.rows;
			}
			ab_append(ab, editor.row[y].line, buflen);
		}
		ab_append(ab, ESCSEQ "K", 3);
		if (y < (editor.rows - 1)) {
			ab_append(ab, "\r\n", 2);
		}
	}
}


void
display_refresh()
{
	char		buf[32];
	struct abuf	ab = ABUF_INIT;

	ab_append(&ab, ESCSEQ "?25l", 6);
	display_clear(&ab);

	draw_rows(&ab);
	
	snprintf(buf, sizeof(buf), ESCSEQ "%d;%dH", editor.cury+1,
		 editor.curx+1);
	ab_append(&ab, buf, strnlen(buf, 32));
	/* ab_append(&ab, ESCSEQ "1;2H", 7); */
	ab_append(&ab, ESCSEQ "?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}


void
loop()
{
	while (1) {
		display_refresh();
		process_keypress();
	}
}


/*
 * init_editor should set up the global editor struct.
 */
void
init_editor()
{
	if (get_winsz(&editor.rows, &editor.cols) == -1) {
		die("can't get window size");
	}

	editor.curx = 0;
	editor.cury = 0;

	editor.nrows = 0;
	editor.row = NULL;
}


int
main(int argc, char *argv[])
{
	setup_terminal();
	init_editor();

	if (argc > 1) {
		open_file(argv[1]);
	}

	display_clear(NULL);
	loop();

	return 0;
}
