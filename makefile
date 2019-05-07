CC=gcc
INCLUDES    = -I. -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
LIB         = -lglib-2.0
CFLAGS=-Wall -g $(INCLUDES)  $(LIB)
TARGET=watchdog
OBJECTS=watchdog.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(GLIB) 

clean:
	rm -rf *.o $(TARGET)
