CC=gcc
FLAGS=-Wall -lm
CFLAGS=-D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags --libs`
DEPS=components.h
OBJECTS= vmufs_driver.o core_components.o display_components.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(FLAGS) $(CFLAGS)

vmufs:	$(OBJECTS)
	$(CC) -o $@ $^ $(FLAGS) $(CFLAGS)

clean:
	/bin/rm -fv *.o $(EXECUTABLE)