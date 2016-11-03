CC=gcc
CFLAGS=-I. -g -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
LIBS = -luuid -lfuse -pthread -lm
DEPS = myfs.h fs.h unqlite.h
OBJ = unqlite.o fs.o
TARGET = myfs

all: $(TARGET)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(TARGET).o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core myfs.* $(TARGET)

