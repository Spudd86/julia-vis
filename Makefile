
CFLAGS = --std=gnu99 -mmmx -msse -mfpmath=sse  -Wall -Wextra \
	-ffast-math -finline-functions -fstrict-aliasing \
	-fsingle-precision-constant \
	\
	-Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
	-Wunreachable-code

CFLAGS = $(CFLAGS) -march=pentium3 -O2 

CFLAGS = `pkg-config --cflags sdl`

all:

gtk-test: src/gtk.c src/map.c src/pallet.c
	gcc -march=pentium3 -mmmx -msse -O2 -ffast-math -finline-functions -fstrict-aliasing -mfpmath=sse --std=gnu99 src/gtk.c src/map.c src/pallet.c `pkg-config --cflags --libs gtk+-2.0 --libs gthread-2.0` -lm -Wall -o gtk-test 
