
CFLAGS = --std=gnu99 -mmmx -msse -mfpmath=sse  -Wall -Wextra \
	-ffast-math -finline-functions -fstrict-aliasing \
	-fsingle-precision-constant \
	\
	-Wpointer-arith -Wmissing-prototypes -Wmissing-field-initializers \
	-Wunreachable-code

CFLAGS = $(CFLAGS) -march=pentium3 -O2 

CFLAGS = `pkg-config --cflags sdl`
