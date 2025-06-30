CC = emcc
SOURCEDIRS = src/ core/src/
TARGET = out/index.html

CCARGS = -Iinclude/ -Icore/include/

SOURCES = $(shell find $(SOURCEDIRS) -name '*.c')

all:
	$(CC) $(CCARGS) $(SOURCES) -o $(TARGET)