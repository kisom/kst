/*
 * kyle's editor
 *
 * first version is a run-through of the kilo editor walkthrough.
 * https://viewsourcecode.org/snaptoken/kilo/
 */
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


#define	KE_VERSION	"0.0.1-pre"
#define ESCSEQ		"\x1b["
#define	CTRL_KEY(key)	((key)&0x1f)
#define TAB_STOP	8
#define MSG_TIMEO	5

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
	char	*render;

	int	 size;
	int	 rsize;
} erow;

struct {
	struct termios	 entry_term;
	int		 rows, cols;
	int		 curx, cury;
	int		 rx;
	int		 mode;
	int		 nrows;
	int		 rowoffs, coloffs;
	erow		*row;
	char		*filename;
	char		 msg[80];
	time_t		 msgtm;
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
update_row(erow *row)
{
	int	i = 0, j;
	int	tabs = 0;

	/*
	 * TODO(kyle): I'm not thrilled with this double-render.
	 */
	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			tabs++;
		}
	}

	free(row->render);
	row->render = malloc(row->size + (tabs * (TAB_STOP-1)) + 1);

	for (j = 0; j < row->size; j++) {
		if (row->line[j] == '\t') {
			do {
				row->render[i++] = ' ';
			} while ((i % TAB_STOP) != 0);
		} else {
			row->render[i++] = row->line[j];
		}
	}

	row->render[i] = '\0';
	row->rsize = i;
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

	editor.row[at].rsize = 0;
	editor.row[at].render = NULL;

	update_row(&editor.row[at]);
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

	free(editor.filename);
	editor.filename = strdup(filename);

	free(line);
	fclose(fp);
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
					case '2': return /* INS_KEY */ c;
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


void
move_cursor(int16_t c)
{
	erow	*row = (editor.cury >= editor.nrows) ?
		       NULL : &editor.row[editor.cury];
	int	 reps;

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
	}

	case HOME_KEY:
	case CTRL_KEY('a'):
		editor.curx = 0;
		break;
	case END_KEY:
	case CTRL_KEY('e'):
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
		break;
	case MODE_NORMAL:
		if (is_arrow_key(c)) {
			move_cursor(c);
		}
	}

	if (c == CTRL_KEY('k')) {
		editor.mode = MODE_KCOMMAND;
		return 1;
	}

	return 1;
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
	int	buflen, filerow, padding;
	int	y;

	for (y = 0; y < editor.rows; y++) {
		filerow = y + editor.rowoffs;
		if (filerow >= editor.nrows) {
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


void
draw_status_bar(struct abuf *ab)
{
	static int	updates = 0;
	char	status[editor.cols];
	char	rstatus[editor.cols];
	int	len, rlen;

	len = snprintf(status, sizeof(status), "%.20s - %d lines - %d updates",
		       editor.filename ? editor.filename : "[no file]",
		       editor.nrows, updates++);
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


int
row_render_to_cursor(erow *row, int cx)
{
	int	rx = 0;
	int	j;

	for (j = 0; j < cx; j++) {
		if (row->line[j] == '\t') {
			rx += (TAB_STOP-1) - (rx%TAB_STOP);
		}
		rx++;
	}

	return rx;
}

void
scroll()
{
	editor.rx = 0;
	if (editor.cury < editor.nrows) {
		editor.rx = row_render_to_cursor(
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
	int	up = 1;

	while (1) {
		if (up) display_refresh();
		up = process_keypress();
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
	editor.rows--; /* status bar */
	editor.rows--; /* message line */

	editor.curx = editor.cury = 0;
	editor.rx = 0;

	editor.nrows = 0;
	editor.rowoffs = editor.coloffs = 0;
	editor.row = NULL;

	editor.msg[0] = '\0';
	editor.msgtm = 0;
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
