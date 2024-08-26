CC = gcc
CFLAGS = -Wall -g -Iinclude 
LDFLAGS = -lm -lrt -lpthread

# Source and object files
SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = testing_files

myprog_src = $(SRC_DIR)/myprog.c
reader_src = $(SRC_DIR)/reader.c
writer_src = $(SRC_DIR)/writer.c

myprog_obj = myprog.o
reader_obj = reader.o
writer_obj = writer.o

EXECUTABLES = myprog reader writer

all: $(EXECUTABLES)

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

myprog: $(myprog_obj)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

reader: $(reader_obj)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

writer: $(writer_obj)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)

run:
	./myprog -dr 3 -dw 3 -f $(TEST_DIR)/accounts50.bin -r ./reader -w ./writer > output.txt

clean:
	rm -f $(myprog_obj) $(reader_obj) $(writer_obj) $(EXECUTABLES)


