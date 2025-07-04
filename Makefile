CC = emcc
SOURCEDIRS = src/ core/src/
TARGET_JS = out/index.js
TARGET_HTML = out/index.html
TEMPLATE = template.html

CCARGS = -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -Iinclude/ -Icore/include/ -O3 -sASYNCIFY

SOURCES = $(shell find $(SOURCEDIRS) -name '*.c')

all: html
	$(CC) $(CCARGS) $(SOURCES) -o $(TARGET_JS)

html:
	cp $(TEMPLATE) $(TARGET_HTML)