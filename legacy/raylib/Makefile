ROOT = ../..
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I. -I$(ROOT)/src
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
TARGET = $(ROOT)/build/spectra_raylib

APP_SRC = main.c \
	$(wildcard ui/*.c) \
	$(wildcard $(ROOT)/src/audio/*.c) \
	$(wildcard $(ROOT)/src/dsp/*.c) \
	$(wildcard $(ROOT)/src/platform/*.c)

DSP_SRC = $(wildcard $(ROOT)/src/dsp/*.c)
TEST_TARGET = $(ROOT)/build/dsp_tests

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(APP_SRC)
	mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) $(APP_SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(ROOT)/tests/dsp_tests.c $(DSP_SRC)
	mkdir -p $(ROOT)/build
	$(CC) $(CFLAGS) $(ROOT)/tests/dsp_tests.c $(DSP_SRC) -o $(TEST_TARGET) -lm

clean:
	rm -rf $(ROOT)/build
