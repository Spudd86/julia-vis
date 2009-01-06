
CFLAGS = --std=gnu99 -D_GNU_SOURCE=1 -mmmx -Wall -fstrict-aliasing -fsingle-precision-constant
#	-Wunsafe-loop-optimizations
	
CFLAGS += -O2 -ffast-math -finline-functions -maccumulate-outgoing-args
CFLAGS += -funroll-loops -fpeel-loops -funswitch-loops -fvariable-expansion-in-unroller -fdelete-null-pointer-checks 
CFLAGS += -ftree-vectorize -ftree-loop-ivcanon -ftree-loop-im -ftree-loop-linear -funsafe-loop-optimizations 
CFLAGS += -fgcse-las -fgcse-sm -fmodulo-sched 
CFLAGS += -fmerge-all-constants

#CFLAGS += -fipa-pta  # bugs on ubuntu

CFLAGS += -fmodulo-sched-allow-regmoves -fgcse-after-reload -fsee -fipa-cp

CFLAGS += -fsched-stalled-insns=2 -fsched-stalled-insns-dep=2
CFLAGS += -fvect-cost-model -ftracer -fassociative-math -freciprocal-math -fno-signed-zeros
#CFLAGS += -fsection-anchors

CFLAGS += `pkg-config --cflags --libs sdl` -lSDL_ttf -lm

-include config.mk

#CFLAGS += -Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
#	-Wunreachable-code

all: bin/sdl-test run

run: bin/sdlthread-test
	bin/sdlthread-test

config.mk:
	touch config.mk

bin/sdl-test: src/sdl.c src/sdl-misc.c src/map.c src/pallet.c src/pixmisc.c src/common.h Makefile config.mk
	$(CC) src/sdl.c src/sdl-misc.c src/map.c src/pallet.c src/pixmisc.c  $(CFLAGS) -o bin/sdl-test

bin/sdlthread-test: src/sdl-thread.c src/sdl-misc.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c src/common.h Makefile config.mk
	$(CC) src/sdl-thread.c src/sdl-misc.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c $(CFLAGS) -o bin/sdlthread-test

%.s: %.c src/*.h Makefile config.mk
	$(CC) $< $(CFLAGS) -S -o $@
