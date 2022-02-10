VERSION = 1.0_fork
PREFIX = /usr/local
CC = cc
CFLAGS = -std=c99 -pedantic -Wall -Wextra -lX11 -lXft -I/usr/include/freetype2 -pthread -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\"
all: herbe

config.h:
	cp -f config.def.h config.h

herbe: herbe.c config.h
	${CC} herbe.c ${CFLAGS} -o herbe

install: herbe
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f herbe ${DESTDIR}${PREFIX}/bin

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/herbe

clean:
	rm -f herbe

.PHONY: all install uninstall clean
