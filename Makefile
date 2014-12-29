RELEASE_VERSION = 0.1

ifndef PREFIX
  PREFIX=/usr/local
endif
ifndef VERSION
  VERSION = $(shell git describe --tags --always 2> /dev/null)
  ifeq ($(strip $(VERSION)),)
    VERSION = $(RELEASE_VERSION)
  endif
endif

PROGRAM = modulo

CPPFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -Wall

OBJS := $(wildcard *.c)
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
	install -m 755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)

.PHONY: all clean install uninstall
