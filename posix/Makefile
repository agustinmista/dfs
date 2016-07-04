# Available debug flags:
# DEBUG: Output main server behaviour
# DEBUG_REQUEST: output DEBUG stuff and inter-worker requests

CFLAGS = -lpthread -lrt -Wall -g -std=c99 -D DEBUG_REQUEST

all: 
	gcc -o dfs_server *.c $(CFLAGS)
	chmod +x dfs_client

clean:
	rm -f *.o
