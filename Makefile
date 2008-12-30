
CFLAGS = --std=gnu99 -mmmx -msse -mfpmath=sse -Wall -Wextra \
	-fstrict-aliasing -fsingle-precision-constant 
	
#CFLAGS += -Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
#	-Wunreachable-code

CFLAGS += -march=native -O2 -ffast-math -finline-functions -fprefetch-loop-arrays
CFLAGS += -funroll-loops -fpeel-loops -funswitch-loops
CFLAGS += `pkg-config --cflags --libs sdl` -lm

#CFLAGS += -ggdb

all:

sdl-test: src/sdl.c src/map.c src/pallet.c src/pixmisc.c Makefile
	gcc src/sdl.c src/map.c src/pallet.c src/pixmisc.c  $(CFLAGS) -o sdl-test 

sdlthread-test: src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c Makefile
	gcc src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c $(CFLAGS) -o sdlthread-test 

