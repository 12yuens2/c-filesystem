CC=gcc
CFLAGS=-I. -g -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse
LIBS = -luuid -lfuse -pthread -lm
DEPS = myfs.h fs.h unqlite.h
OBJ = unqlite.o fs.o
TARGET1 = store
TARGET2 = fetch
TARGET3 = myfs
TARGET4 = test
TARGET5 = uuid

all: $(TARGET3) $(TARGET4) $(TARGET5)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET1): $(TARGET1).o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

$(TARGET2): $(TARGET2).o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)
	
$(TARGET3): $(TARGET3).o $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)
	
$(TARGET4): $(TARGET4).c
	gcc -o test test.c

$(TARGET5): $(TARGET5).c
	gcc -o uuid uuid.c -luuid

.PHONY: clean

clean:
	rm -f *.o *~ core myfs.db myfs.log $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET5)

