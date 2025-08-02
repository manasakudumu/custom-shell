CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.c)
TARGET = custom-shell-c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TARGET)
