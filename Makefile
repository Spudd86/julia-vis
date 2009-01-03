
CFLAGS = --std=gnu99 -mmmx  -Wall \
	-fstrict-aliasing -fsingle-precision-constant 
#	-Wunsafe-loop-optimizations
	
CFLAGS += -O3 -ffast-math -finline-functions -maccumulate-outgoing-args
CFLAGS += -funroll-loops -fpeel-loops -funswitch-loops -fvariable-expansion-in-unroller -fdelete-null-pointer-checks 
CFLAGS += -ftree-vectorize -ftree-loop-ivcanon -ftree-loop-im -ftree-loop-linear -funsafe-loop-optimizations 
CFLAGS += -fgcse-las -fgcse-sm -fmodulo-sched 
CFLAGS += -fmerge-all-constants

CFLAGS += -fmodulo-sched-allow-regmoves -fgcse-after-reload -fsee -fipa-pta -fipa-cp
CFLAGS += -fsched-stalled-insns -fsched-stalled-insns-dep
CFLAGS += -fvect-cost-model -ftracer -fassociative-math -freciprocal-math -fno-signed-zeros
#CFLAGS += -fsection-anchors

CFLAGS += `pkg-config --cflags --libs sdl` -lm

#CFLAGS += -DTILED

-include config.mk

#CFLAGS += -Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
#	-Wunreachable-code

all: run

run: sdlthread-test
	./sdlthread-test

config.mk:
	touch config.mk

sdl-test: src/sdl.c src/map.c src/pallet.c src/pixmisc.c src/common.h Makefile config.mk
	$(CC) src/sdl.c src/map.c src/pallet.c src/pixmisc.c  $(CFLAGS) -o sdl-test 

sdlthread-test: src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c src/common.h Makefile config.mk
	$(CC) src/sdl-thread.c src/map.c src/pallet.c src/pixmisc.c src/tribuf.c $(CFLAGS) -o sdlthread-test 

%.s: %.c src/*.h Makefile config.mk
	$(CC) $< $(CFLAGS) -S -o $@
