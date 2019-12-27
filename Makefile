CC=gcc
FLAGS=-Wall -lm
CFLAGS=-D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags --libs`
DEPS=libfuman.h
OBJECTS= moofs_driver.o core_components.o display_components.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(FLAGS) $(CFLAGS)

moofs:	$(OBJECTS)
	$(CC) -o $@ $^ $(FLAGS) $(CFLAGS)

clean:
	/bin/rm -fv *.o $(EXECUTABLE)