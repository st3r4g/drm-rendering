DEPFILES=algebra.h backend.h renderer.h session.h
OBJFILES=algebra.o backend.o renderer.o session.o main.o

CC=gcc

IDIR=include
SDIR=src
ODIR=obj

OPTS=-std=c11 -Wall -Wextra -Werror -pedantic-errors -Wno-unused

CFLAGS=$(OPTS) -I$(IDIR)

DEPS=$(patsubst %,$(IDIR)/%,$(DEPFILES))

OBJ=$(patsubst %,$(ODIR)/%,$(OBJFILES))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) -I/usr/include/libdrm

main: $(OBJ)
	$(CC) -o $@ $^ $(ODIR)/stb_image.o -lm -ludev -linput -lsystemd -ldrm -lgbm -lEGL -lGLESv2

.PHONY: clean

clean:
	rm main $(ODIR)/*.o

stb_image:
	$(CC) -c -o $(ODIR)/stb_image.o $(SDIR)/stb_image.c -I$(IDIR)
