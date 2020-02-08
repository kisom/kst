#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>


#define ESCSEQ		"\x1b["
#define	CTRL_KEY(key)	((key)&0x1f)

/*
 * define the keyboard input modes
 * normal: no special mode
 * kcommand: ^k commands
 */
#define	MODE_NORMAL	0
#define	MODE_KCOMMAND	1


struct {
	struct termios	entry_term;
	int		rows, cols;
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


static char
get_keypress()
{
	char	c = 0;

	if (read(STDIN_FILENO, &c, 1) == -1) {
		die("get_keypress:read");
	}

	return c;
}


void
process_keypress()
{
	static int	m = MODE_NORMAL;
	char		c = get_keypress();

	if (c == 0x0) {
		return;
	}

	switch (m) {
	case MODE_KCOMMAND:
		switch (c) {
		case 'q':
		case CTRL_KEY('q'):
			exit(0);
		}

		m = MODE_NORMAL;
		return;
	}

	if (c == CTRL_KEY('k')) {
		m = MODE_KCOMMAND;
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


static void
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
draw_rows(struct abuf *ab)
{
	int	y;

	for (y = 0; y < editor.rows-1; y++) {
		ab_append(ab, "|\r\n", 3);
	}
	ab_append(ab, "|", 1);
}


void
display_refresh()
{
	struct abuf	ab = ABUF_INIT;

	ab_append(&ab, ESCSEQ "?25l", 6);
	display_clear(&ab);
	draw_rows(&ab);
	ab_append(&ab, ESCSEQ "1;2H", 7);
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
}


int
main()
{
	setup_terminal();
	init_editor();

	display_clear(NULL);
	loop();

	return 0;
}
