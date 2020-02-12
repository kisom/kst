BIN :=		kte
OBJS :=		main.o terminal.o input.o


LDFLAGS :=	-lncurses
CFLAGS :=       -pedantic -Wall -Werror -Wextra -O0 -std=c99 -g

.PHONY: all
all: build

.PHONY: build
build: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJS) *.core 

.PHONY: run
run: $(BIN)
	reset
	./$(BIN) hello.txt

.PHONY: install
install: $(BIN)
	cp $(BIN) $(HOME)/bin/

%.o: %.c
	$(CC) $(CFLAGS) -c $<