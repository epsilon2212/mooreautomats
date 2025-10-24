CC = gcc
CFLAGS = -g -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS = -shared -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup
TARGET = libma.so
SRC = ma.c memory_tests.c
OBJ = $(SRC:.c=.o)
EXAMPLE = ma_example
EXAMPLE_SRC = ma_example.c

.PHONY: all clean

all: $(TARGET) $(EXAMPLE)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c ma.h
	$(CC) $(CFLAGS) -c $< -o $@

$(EXAMPLE): $(EXAMPLE_SRC) $(TARGET)
	$(CC) $(CFLAGS) -o $@ $< -L. -lma -Wl,-rpath=.

clean:
	rm -f $(OBJ) $(TARGET) $(EXAMPLE)
