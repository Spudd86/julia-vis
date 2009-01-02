
CFLAGS = --std=gnu99 -mmmx -msse -mfpmath=sse -Wall -Wextra \
	-fstrict-aliasing -fsingle-precision-constant -Wunsafe-loop-optimizations
	
#CFLAGS += -Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
#	-Wunreachable-code

#CFLAGS += -march=native 
CFLAGS += -march=pentium3
CFLAGS += -O2 -ffast-math -finline-functions -maccumulate-outgoing-args
#CFLAGS += -pg
#CFLAGS += -fprofile-generate
#CFLAGS += -fprofile-use 
# -fprefetch-loop-arrays
CFLAGS += -funroll-loops -fpeel-loops -funswitch-loops -fvariable-expansion-in-unroller -fdelete-null-pointer-checks 
CFLAGS += -ftree-vectorize -ftree-loop-ivcanon -ftree-loop-im -ftree-loop-linear -funsafe-loop-optimizations -fgcse-las -fgcse-sm -fmodulo-sched
CFLAGS += -fmerge-all-constants
CFLAGS += `pkg-config --cflags --libs sdl` -lm

#CFLAGS += -ggdb

all: sdlthread-test
	./sdlthread-test

sdl-test: src/sdl.c src/map.c src/pallet.c src/pixmisc.c Makefile
	gcc src/sdl.c src/map.c src/pallet.c src/pixmisc.c  $(CFLAGS) -o sdl-test 

sdlthread-test: src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c Makefile
	gcc src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c $(CFLAGS) -o sdlthread-test 

