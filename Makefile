# ablocks - i3bar status command with animated blocks
# See LICENSE file for copyright and license details.

include config.mk

SRC = ablocks.c
OBJ = ${SRC:.c=.o}

all: ablocks icons/suckless-icons.otf

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

config.h:
	cp config.def.h $@

ablocks: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

icons/suckless-icons.otf: icons/icons.py
	python3 icons/icons.py build $@

clean:
	rm -f ablocks ${OBJ} icons/suckless-icons.otf

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ablocks ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/ablocks
	mkdir -p ${DESTDIR}${PREFIX}/share/fonts/suckless-icons
	cp -f icons/suckless-icons.otf ${DESTDIR}${PREFIX}/share/fonts/suckless-icons
	chmod 644 ${DESTDIR}${PREFIX}/share/fonts/suckless-icons/suckless-icons.otf

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/ablocks
	rm -rf ${DESTDIR}${PREFIX}/share/fonts/suckless-icons

.PHONY: all clean install uninstall
