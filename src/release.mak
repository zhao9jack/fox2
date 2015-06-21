# Makefile for CWebServer

CC=		gcc -O2
CFLAGS=	-c -s -Wall
LDFLAGS=	-lws2_32 -lpthreadGC2 -lregex2 -o output -L. -s
LD=		gcc -O2

OBJS=		main.o server.o debug.o config.o memory.o client.o connection.o session.o http.o utf8.o \
	util.o plugin.o loop.o vdir.o xmlparser.o proxy.o listdir.o

TARGET=	cwebserver.exe

all: $(TARGET)
	@echo done.
$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o $@


.c.o:
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm *.o -f
	rm $(TARGET) -f
