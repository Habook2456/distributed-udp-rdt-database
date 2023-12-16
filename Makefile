CC = g++
CFLAGS = -std=c++11

all: server client storage prg

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server

client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client

storage: storage.cpp
	$(CC) $(CFLAGS) storage.cpp -o storage

prg: prg.cpp
	$(CC) $(CFLAGS) prg.cpp -o prg.exe

clean:
	rm -f server client storage prg.exe
