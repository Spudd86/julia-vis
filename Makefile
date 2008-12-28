
CFLAGS = --std=gnu99 -mmmx -msse -mfpmath=sse  -Wall -Wextra \
	-fstrict-aliasing -fsingle-precision-constant 
	
#CFLAGS += -Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
#	-Wunreachable-code

CFLAGS += -march=pentium3 -O2 -ffast-math -finline-functions 
CFLAGS += `pkg-config --cflags --libs sdl` -lm

#CFLAGS += -ggdb

all:

sdl-test: src/sdl.c src/map.c src/pallet.c
	gcc src/sdl.c src/map.c src/pallet.c $(CFLAGS) -o sdl-test 

