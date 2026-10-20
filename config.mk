# ablocks version
VERSION = 0.1

# paths
PREFIX = /usr/local

# flags
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Os ${CPPFLAGS}
LDFLAGS  =

# compiler and linker
CC = cc
