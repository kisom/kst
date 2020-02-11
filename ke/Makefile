BIN :=		ke
OBJS :=		main.o

LDFLAGS :=
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O0 -std=c99 -g 

.PHONY: all
all: build 

.PHONY: build
build: $(BIN)

$(BIN): main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

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
	cp $(BIN) $(HOME)/bin/

%.o: %.c

