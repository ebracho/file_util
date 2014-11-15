all: file_client file_server

file_client: file_client.c
	gcc file_client.c -Wall -pedantic -std=gnu99 -o file_client

file_server: file_server.c
	gcc file_server.c -Wall -pedantic -std=gnu99 -o file_server
