#include <stdlib.h>
#include <string.h>

#include "defs.h"


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
