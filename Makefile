OBJECTS=mesh.o mesh_prog.o parser.o
CC=colorgcc

CFLAGS=-g -Wall -pipe

default: mesh_prog

mesh_prog: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f mesh_prog $(OBJECTS)
