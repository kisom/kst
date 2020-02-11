BIN :=		ke
OBJS :=		main.o

LDFLAGS :=	
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O2 -std=c99 -g 

.PHONY: all
all: build run

.PHONY: build
build: $(BIN)

$(BIN): main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJS) *.core

.PHONY: run
run: $(BIN)
	reset
	./$(BIN) notes.txt

keypress: keypress.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ keypress.c

%.o: %.c
	$(CC) $(CFLAGS) -c $@ $<
