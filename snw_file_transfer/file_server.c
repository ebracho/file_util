/*file_server.c
* Eddie Bracho
* Taber Storm Fitzgerald
* CSCI 446
* Fall 2014*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "packetErrorSend.h"

#define LINE_LEN 1000
#define FILE_ERR "file_err"
#define ERR_LEN strlen(FILE_ERR) + 1
#define ERR -1
const int HEADER = 2;
/*
	recvName:
	this function recives a name as a string and acks back. It returns an int -1 for error
*/
int recvName(int sockfd, char* buf)
{
    char* ack;
    int mesLen = 0;
    ack = (char*) malloc(1);
    ack[0] = 'a';
    int offset = 0;
    int bytesRead = 0;
    bytesRead = read(sockfd, buf+offset, LINE_LEN+HEADER);
    offset += bytesRead;
    mesLen = (unsigned int) buf[1];
    while( bytesRead == 0 || offset < mesLen )
    {
        bytesRead = read(sockfd, buf+offset, LINE_LEN);
        if(bytesRead == ERR)
        {
        	return ERR;
        }
        offset += bytesRead;
    }
    packetErrorSend(sockfd, ack, 1, 0);
    return 0;
    
}
/*
	sendChunk:
	this function sends the file data to the client. We used 3 seconds as our time out value. it seemed like an alright number to use. 3 seconds isnt very long to wait, and youll recieve a responce sooner than 3 sec. It returns an int which is -1 if it encountered an error
*/
int sendChunk(int sockfd, const char* buf, int len, unsigned int head)
{
    char* newBuf;
    newBuf = (char*) malloc(len+HEADER);
    newBuf[0] = head;
    newBuf[1] = len;
    memcpy(newBuf+HEADER, buf, len);
    int ackRecv = 0;
    int error = 0;
    char* ack = (char*) malloc(1);
    fd_set readfds;
    struct timeval timeout;
    int retval = 0;
    while(!retval)
    {
        error = packetErrorSend(sockfd, newBuf, len+HEADER, 0);
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        retval = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        if((error == ERR) || (retval == ERR))//if there is an error return and tell main
        {
            return ERR;
        }
        if(retval > 0)
        {
            while((ackRecv = read(sockfd, ack, LINE_LEN)))//get ack contents
            {
            	if(ack[0] == 'a')
            	{
            		break;
            	}
            }
        }  
    }
    free(newBuf);
    free(ack);
    return 0;
}

int main(int argc, char** argv)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char* port;
    char* fcontents;
    char* filename;
    int err, s, new_s/*, size*/;
    int debug;

    debug = 0;

    if (argc == 2)
    {
        port = argv[1];
    }
    else if (argc == 3 && strcmp(argv[2], "-d") == 0)
    {
        port = argv[1];
        debug = 1; 
    }
    else
    {
        fprintf(stderr, "Usage: %s <port> {-d}\n", argv[0]);
        exit(1);
    }

    /* Build address data structure */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    /* Get local address info */
    if ((err = getaddrinfo(NULL, port, &hints, &res)) != 0 )
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", argv[0], gai_strerror(err));
        exit(1);
    }

    /* Iterate through the address list and try to perform passive open */
    if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == ERR )
    {
        perror("file_server: socket");
        exit(1);
    }

    if (bind(s, res->ai_addr, res->ai_addrlen) == ERR)
    {
        perror("file_server: bind");
        close(s);
        exit(1);
    }

    if (listen(s, 1) == ERR)
    {
        perror("file_server: listen");
        close(s);
        exit(1);
    }

    if ((new_s = accept(s, res->ai_addr, &(res->ai_addrlen))) < 0)
    {
        perror("file_server: accept");
        close(s);
        exit(1);
    }

    /* Recieve filename from client */
    filename = (char*) malloc(LINE_LEN);
    if (recvName(new_s, filename) == ERR)
    {
        perror("host_recv()");
        close(s);
        close(new_s);
        exit(1);
    }
    

    if (debug) printf("Filename: %s\n", filename);

    /* Open and read file. If errors occur, set fcontents to error value */
    int fd, bytesRead;
    if ((fd = open(filename+HEADER, O_RDONLY, 0)) == ERR)
    {
        perror("open()");
        close(s);
        close(new_s);
        exit(1);/* open error */
    }
    unsigned int head = 0;
    fcontents = (char*) malloc(LINE_LEN);
    while((bytesRead = read(fd, fcontents, LINE_LEN)))
    {
        if(bytesRead == ERR)
        {
            perror("read");
            close(s);
        	close(new_s);
       		exit(1);
        }
        if(sendChunk(new_s, fcontents, bytesRead, head) == ERR)
        {
            perror("send");
            close(s);
        	close(new_s);
        	exit(1);
        }
        free(fcontents);
        fcontents = (char*) malloc(LINE_LEN);
        head++;
        
    }
    char fin[2];
    fin[0] = head;
    fin[1] = 0;
    packetErrorSend(new_s, fin, 2, 0);
    free(fcontents);

    if (debug) printf("file contents sent\n");
    free(filename);
    freeaddrinfo(res);
    close(s);
    close(new_s);

    return 0;
}
