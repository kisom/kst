BIN :=		ke
OBJS :=		main.o
INSTROOT :=	$(HOME)
VERSION :=	0.9.2

LDFLAGS :=
CFLAGS :=	-pedantic -Wall -Werror -Wextra -O2 -std=c99 -g
CFLAGS +=	-fno-builtin-memmove -DKE_VERSION="\"$(VERSION)\""

.PHONY: all
all: build

.PHONY: build
build: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BIN).1.txt: $(BIN).1
	mandoc -Tutf8 $(BIN).1 > $@

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
install: $(BIN) $(BIN).1
	install -d $(INSTROOT)/bin/
	install -d $(INSTROOT)/share/man/man1
	install -C $(BIN) $(INSTROOT)/bin/
	install -C $(BIN).1 $(INSTROOT)/share/man/man1/$(BIN).1

.PHONY: upload
upload: $(BIN).1.txt
	scp main.c p.kyleisom.net:/var/www/sites/p/ke/$(BIN)_$(VERSION).c.txt
	scp $(BIN).1.txt p.kyleisom.net:/var/www/sites/p/ke/$(BIN).1.txt

.PHONY: cloc
cloc:
	cloc main.c

%.o: %.c
