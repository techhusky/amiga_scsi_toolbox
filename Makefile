# Makefile for building toolbox with vbcc
#
# Targets the Amiga m68k platform using vbcc cross-compiler.
# Adjust CC, TARGET, and CPU as needed for your setup.

CC = vc
TARGET = +aos68k
CPU = -cpu=68000

CFLAGS = $(TARGET) $(CPU) -c99
LDFLAGS = $(TARGET) $(CPU) -lamiga

TOOLBOX_DEPS = toolbox_version.h toolbox_rev.h

.PHONY: all clean dist

all: toolbox

toolbox: toolbox.o
	$(CC) $(LDFLAGS) -o $@ $<

toolbox.o: toolbox.c $(TOOLBOX_DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

toolbox.readme: toolbox.readme.in $(TOOLBOX_DEPS)
	$(CC) $(TARGET) -E $< -o $@

toolbox.guide: toolbox.guide.in $(TOOLBOX_DEPS)
	$(CC) $(TARGET) -E $< -o $@

clean:
	-rm -f toolbox.o toolbox toolbox.readme toolbox.guide toolbox.lha
	-rm -rf lha

toolbox.lha: toolbox toolbox.guide
	-rm -rf toolbox.lha lha
	mkdir -p lha/toolbox
	cp toolbox lha/toolbox/
	cp toolbox.guide lha/toolbox/
	lha -r a toolbox.lha lha/

dist: toolbox.readme toolbox.lha
