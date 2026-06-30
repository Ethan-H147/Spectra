CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -Isrc
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = build/spectra_desktop

APP_SRC = src/main.c \
	$(wildcard src/audio/*.c) \
	$(wildcard src/dsp/*.c) \
	$(wildcard src/ui/*.c)

DSP_SRC = $(wildcard src/dsp/*.c)
TEST_TARGET = build/dsp_tests

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(APP_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(APP_SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): tests/dsp_tests.c $(DSP_SRC)
	mkdir -p build
	$(CC) $(CFLAGS) tests/dsp_tests.c $(DSP_SRC) -o $(TEST_TARGET) -lm

clean:
	rm -rf build
