all: snw-client snw-server

snw-client: file_client.c
	gcc file_client.c -Wall -pedantic -std=gnu99 -LpacketErrorSend libpacketErrorSend.a -o snw-client

snw-server: file_server.c
	gcc file_server.c -Wall -pedantic -std=gnu99 -LpacketErrorSend libpacketErrorSend.a -o snw-server
