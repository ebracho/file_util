/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define LINE_LEN 256
#define FILE_ERR "file_err"
#define ERR_LEN strlen(FILE_ERR) + 1

/* Send the contents of buf to the client */
int client_send(int sockfd, const char* buf, int size)
{
    int bytes_sent, total;
    
    total = 0;
    while (total < size)
    {
        if ((bytes_sent = send(sockfd, buf+total, size-total, 0)) == -1)
        {
            return -1;
        }
        total += bytes_sent;
    }
    printf ("Sent %d bytes\n", total);
    return 0;
}

/* Recieve message from client until it times out */
int client_recv(int sockfd, char **res)
{
    fd_set readfds;
    struct timeval tv;
    int bytes_recieved, total, retval;

    total = 0;
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        retval = select(sockfd+1, &readfds, NULL, NULL, &tv);

        *res = realloc(*res, total + LINE_LEN);
    
        if (retval == -1) /* select error */
        {
            return -1;
        }
        if (retval == 0) /* client timed out */
        {
            break;
        }
        if ((bytes_recieved = recv(sockfd, (*res)+total, LINE_LEN, 0)) == -1) 
        {
            return -1; /* recv error */
        }

        total += bytes_recieved;
    }
    return total;
}

int read_file(char *filename, char **res)
{
    int fd, bytes_read, total;
    char *buf = NULL;

    if ((fd = open(filename, O_RDONLY, 0)) == -1)
    {
        return -1; /* open error */
    }

    total = 0;
    do
    {
        buf = realloc(buf, total + LINE_LEN);
        if ((bytes_read = read(fd, buf+total, LINE_LEN)) == -1)
        {
            return -1; /* read error */
        }
        total += bytes_read;
    } while (bytes_read != 0);

    *res = malloc(total);
    memcpy(*res, buf, total);

    close(fd);
    return total;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints;
    struct addrinfo *rp, *result;
    char *port, *fcontents, *filename;
    int s, new_s, size;
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
    if ((s = getaddrinfo(NULL, port, &hints, &result)) != 0 )
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", argv[0], gai_strerror(s));
        exit(1);
    }

    /* Iterate through the address list and try to perform passive open */
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1 )
        {
            continue;
        }

        if (!bind(s, rp->ai_addr, rp->ai_addrlen))
        {
            break;
        }

        close(s);
    }
    if (rp == NULL)
    {
        perror("file_server: bind");
        exit(1);
    }
    if (listen(s, 1) == -1)
    {
        perror("file_server: listen");
        close(s);
        exit(1);
    }

    if ((new_s = accept(s, rp->ai_addr, &(rp->ai_addrlen))) < 0)
    {
        perror("file_server: accept");
        close(s);
        exit(1);
    }

    /* Recieve filename from client */
    if (client_recv(new_s, &filename) == -1)
    {
        perror("client_recv()");
        close(s);
        close(new_s);
        exit(1);
    }

    if (debug) printf("Filename: %s\n", filename);

    /* Open and read file. If errors occur, set fcontents to error value */
    if ((size = read_file(filename, &fcontents)) == -1)
    {
        perror("read_file()");
        fcontents = FILE_ERR;
        size = ERR_LEN;
    }

    if (debug) printf("File Contents:\n%s\n", fcontents);

    /* Send contents (or error value) to client */
    if (client_send(new_s, fcontents, size) == -1)
    {
        perror("client_send()");
        close(s);
        close(new_s);
        exit(1);
    }

    if (debug) printf("file contents sent\n");

    freeaddrinfo(result);
    close(s);
    close(new_s);

    return 0;
}
