# ablocks - i3bar status command with animated blocks
# See LICENSE file for copyright and license details.

include config.mk

SRC = ablocks.c
OBJ = ${SRC:.c=.o}

all: ablocks

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

ablocks: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ablocks ${OBJ}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ablocks ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ablocks

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ablocks

.PHONY: all clean install uninstall
