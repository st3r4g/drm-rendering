_DEPS=mysession.h mydrm.h myegl.h
_OBJ=main.o session.o drm.o egl.o

CC=gcc

IDIR=include
SDIR=src
ODIR=obj

OPTS=-std=c11 -Wall -Wextra -Werror -pedantic-errors

CFLAGS=$(OPTS) -I$(IDIR)

DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -I/usr/include/libdrm

main: $(OBJ)
	$(CC) -o $@ $^ -lsystemd -ldrm -lgbm -lEGL

.PHONY: clean

clean:
	rm -f main $(ODIR)/*.o
