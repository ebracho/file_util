/*file_client.c 
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
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "packetErrorSend.h"

#define LINE_LEN 1000
#define FILE_ERR "file_err"
#define ERR_LEN strlen(FILE_ERR) + 1
#define ERR -1
const int HEADER = 2;

/*
	sendName:
	this function sends the name of a file as a string to the server. We used 3 seconds as our time out value. it seemed like an alright number to use. 3 seconds isnt very long to wait, and youll recieve a responce sooner than 3 sec. It returns an int which is -1 if it encountered an error
*/
int sendName(int sockfd, const char* buf, int len, unsigned int head)
{
    char* newBuf;
    newBuf = (char*) malloc(len+HEADER);
    newBuf[0] = head;
    newBuf[1] = len;
    strcpy(newBuf+HEADER, buf);
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
            while((ackRecv = read(sockfd, ack, 1)))//get ack contents
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
/*
	recvAndWrite:
	this function recieves data from the server and acks  back when it get it, then writes the data to a file. it returns an int to describe the state. 0 for continue, 1 for done, -1 for err
*/
int recvAndWrite(int sockfd, char* buf, int fd, unsigned int* checkHead)
{
    buf = (char*) malloc(LINE_LEN + 2);
    char* ack;
    int mesLen = 0;
    int bytes_written = 0;
    unsigned int total = 0;
    ack = (char*) malloc(1);
    ack[0] = 'a';
    int offset = 0;
    int bytesRead = 0;

    bytesRead = read(sockfd, buf+offset, LINE_LEN+2);
    offset += bytesRead;
    if(*checkHead != ((unsigned int)buf[0]))
    {
    	packetErrorSend(sockfd, ack, 1, 0);
    	return 0;
    }
    mesLen = (unsigned int) buf[1];
    if(mesLen == 0)
    {
    	return 1;
    }
    while( bytesRead == 0 || offset < mesLen )
    {
        bytesRead = read(sockfd, buf+offset, LINE_LEN);
        offset += bytesRead;
    }
    packetErrorSend(sockfd, ack, 1, 0);
    while ((bytes_written = write(fd, buf+HEADER+total, offset-total-HEADER)))
    {
        if (bytes_written == ERR)
        {
            return ERR;
        }
        total += bytes_written;
    }
	(*checkHead)++;
    return 0;
}


int main(int argc, char** argv)
{
    struct addrinfo hints;
    struct addrinfo *res;
    char* host;
    char* port;
    char* file;
    char* fcontents;
    int err, s, size;

    if (argc==4)
    {
        host = argv[1];
        port = argv[2];
        file = argv[3];
        
        if (strlen(file) > (LINE_LEN - 1))
        {
            fprintf(stderr, "File name cannot exceed %d chars\n", LINE_LEN-1);
            exit(1);
        }
    }
    else
    {
        fprintf(stderr, "usage: %s host\n", argv[0]);
        exit(1);
    }

    /* Translate host name into peer's IP address */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    if ((err = getaddrinfo(host, port, &hints, &res)) != 0 )
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", argv[0], gai_strerror(err));
        exit(1);
    }

    if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == ERR )
    {
        perror("file_client: socket");
        exit(1);
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) == ERR)
    {
        perror("file_client: connect");
        close(s);
        exit(1);
    }
    
    /* Send the file name to the server */
    unsigned int head = 0;
    if (sendName(s, file, strlen(file)+1, head) == ERR) 
    {
        perror("sendName()");
        close(s);
        exit(1);
    }

    /* Recieve the server's response */
    int fd;
    if ((fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == ERR)
    {
        return -1; /* open error */
    }
    fcontents = NULL;
    unsigned int checkHead = 0;
    do
    {
    	if ((size = recvAndWrite(s, fcontents, fd, &checkHead)) == ERR)
    	{
        	perror("recvAndWrite()");
        	close(s);
        	exit(1);
   		}
    }while(size != 1);
    close(fd);
    

    /* Handle error from server */
    /*if (strcmp(fcontents, FILE_ERR) == 0)
    {
        fprintf(stderr, "Server: Error opening file.\n");
        close(s);
        exit(1);
    }*/

    
    freeaddrinfo(res);
    close(s);

    return 0;
}
