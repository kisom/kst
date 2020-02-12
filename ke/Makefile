BIN :=		ke
OBJS :=		abuf.o erow.o main.o

LDFLAGS :=
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O2 -std=c99 -g -fno-builtin-memmove

.PHONY: all
all: build install

.PHONY: build
build: $(BIN)

$(BIN): $(OBJS) defs.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJS) *.core keypress

.PHONY: run
run: $(BIN)
	reset
	./$(BIN) hello.txt

keypress: keypress.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ keypress.c

.PHONY: install
install: $(BIN)
	install -C $(BIN) $(HOME)/bin/

%.o: %.c
