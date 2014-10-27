ifndef PREFIX
  PREFIX=/usr/local
endif
ifndef SYSCONFDIR
  ifeq ($(PREFIX),/usr)
    SYSCONFDIR=/etc
  else
    SYSCONFDIR=$(PREFIX)/etc
  endif
endif

PROGRAM = modulo
VERSION = "$(shell git describe --tags --always)"

CPPFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -Wall

OBJS := $(wildcard src/*.c *.c)
OBJS := $(OBJS:.c=.o)

%.o: %.c %.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: $(PROGRAM)

debug: CPPFLAGS += -DDEBUG
debug: CFLAGS += -g
debug: $(PROGRAM)

$(PROGRAM): ${OBJS}
	$(CC) $(LDFLAGS) -o $@ $^
	@echo " LD $@"

clean:
	rm -f *.o $(PROGRAM)

doc:
	make -C doc

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(PREFIX)/libexec/$(PROGRAM)
	install -m 755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	rm -rf $(DESTDIR)$(PREFIX)/libexec/$(PROGRAM)

.PHONY: all clean install uninstall
