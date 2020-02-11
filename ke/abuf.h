#ifndef KE_ABUF_H
#define KE_ABUF_H

struct abuf {
	char	*b;
	int	 len;
};

#define ABUF_INIT	{NULL, 0}


void	ab_append(struct abuf *buf, const char *s, int len);
void	ab_free(struct abuf *buf);



#endif /* KE_ABUF_H */
