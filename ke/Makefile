BIN :=		ke
OBJS :=		main.o		\
		display.o	\
		keys.o		\
		terminal.o	\
		util.o

LDFLAGS :=	-static
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O2 -std=c99

.PHONY: all
all: $(BIN) run

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.PHONY: clean
clean:
	rm -f $(BIN) $(OBJS)

.PHONY: run
run: $(BIN)
	reset
	./$(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $@ $<
