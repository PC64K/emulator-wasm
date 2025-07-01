CC = emcc
SOURCEDIRS = src/ core/src/
TARGET = out/index.html

CCARGS = -sUSE_SDL=2 -Iinclude/ -Icore/include/

SOURCES = $(shell find $(SOURCEDIRS) -name '*.c')

all:
	$(CC) $(CCARGS) $(SOURCES) -o $(TARGET)