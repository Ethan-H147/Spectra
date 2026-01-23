# The compiler
CC = gcc

# The flags (tell it to show warnings)
CFLAGS = -Wall -std=c99

# The libraries we need to link (Raylib + Linux Graphics)
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# The target (our executable name)
TARGET = c_spectra

# The rule: "To build the target, compile main.c with these flags"
$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LDFLAGS)

# Cleanup command
clean:
	rm -f $(TARGET)