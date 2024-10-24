# Makefile for Windows using MinGW
CC = gcc
CFLAGS = -O2
LDFLAGS = -s
TARGET = word_counter.exe
SRC = wordCounter.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

.PHONY: clean
clean:
	del /F /Q $(TARGET)

.PHONY: all
all: $(TARGET)