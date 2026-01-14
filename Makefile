CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11

all: mini-webserver

mini-webserver: main.c
	$(CC) $(CFLAGS) main.c -o mini-webserver

clean:
	rm -f mini-webserver
