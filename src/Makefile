LDLIBS=-lxcb -lxcb-shape -lxcb-render

CFLAGS=-g -Wall

.PHONY: clean

aawm: get_constant_string.o utfconv.o map.o aawm_window.o atoms.o

clean:
	$(RM) *.o aawm

