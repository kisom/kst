#include "defs.h"


void
init_buffer(struct buffer *buf)
{
	/* put the cursor at the top of the file */
	buf->curx = 0;
	buf->cury = 0;
}
