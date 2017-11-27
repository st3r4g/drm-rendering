_DEPS=mysession.h mydrm.h myrenderer.h
_OBJ=main.o session.o drm.o renderer.o

CC=gcc

IDIR=include
SDIR=src
ODIR=obj

OPTS=-std=c11 -Wall -Wextra -Werror -pedantic-errors -Wno-unused

CFLAGS=$(OPTS) -I$(IDIR)

DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SDIR)/%.c $(DEPS) $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS) -I/usr/include/libdrm

main: $(OBJ)
	$(CC) -o $@ $^ -ludev -lsystemd -ldrm -lgbm -lEGL -lGLESv2

$(ODIR):
	mkdir $(ODIR)

.PHONY: clean

clean:
	rm main $(ODIR)/*.o
	rm -d $(ODIR)
