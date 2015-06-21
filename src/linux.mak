# Makefile for CWebServer

CC=		gcc
CFLAGS=	-c -g -Wall
LDFLAGS=	-lpthread -ldl  -L.
LD=		gcc

OBJS=		main.o server.o debug.o config.o memory.o client.o connection.o session.o \
	http.o utf8.o util.o plugin.o vdir.o loop.o xmlparser.o proxy.o listdir.o

TARGET=	cwebserver

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
