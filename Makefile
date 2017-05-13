
CC := gcc
CFLAGS = -Wall -Wpedantic -g
LIBS = -L../iniparser -liniparser -pthread
INCLUDE = -I../iniparser/src

all: client server

client: client.c
	$(CC) $(CFLAGS) -o $@ $<

server: server.o
	@$(CC) -o $@ $< $(LIBS) 

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

.PHONY: clean

clean:
	-rm *.o client server 
