/* This code is an updated version of the sample code from "Computer Networks: A Systems
 * Approach," 5th Edition by Larry L. Peterson and Bruce S. Davis. Some code comes from
 * man pages, mostly getaddrinfo(3). */
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

#define LINE_LEN 256
#define FILE_ERR "file_err"
#define ERR_LEN strlen(FILE_ERR) + 1

/* Send the contents of buf to the server */
int serv_send(int sockfd, const char* buf, int size)
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
    return 0;
}

/* Recieve message from server until it times out. Returns # bytes recieved */
int serv_recv(int sockfd, char **res)
{
    fd_set readfds;
    struct timeval tv;
    int bytes_recieved, total, retval;

    *res = NULL;
    total = 0;
    bytes_recieved = 1;
    while (bytes_recieved != 0)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        tv.tv_sec = 5;  /* Twice as long as the server waits*/ 
        tv.tv_usec = 0; /* so it doesn't miss the response. */
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
        if ((bytes_recieved = recv(sockfd, *res+total, LINE_LEN, 0)) == -1) 
        {
            return -1; /* recv error */
        }

        total += bytes_recieved;
    }
    return total;
}

int write_file(char *filename, char *buf, int size)
{
    int fd, bytes_written, total;

    if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
    {
        return -1; /* open error */
    }
    
    while (total < size)
    {
        if ((bytes_written = write(fd, buf+total, size-total)) == -1)
        {
            return -1;
        }
        total += bytes_written;
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints;
    struct addrinfo *rp, *result;
    char *host, *port, *file;
    char *fcontents;
    int s, size;

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

    if ((s = getaddrinfo(host, port, &hints, &result)) != 0 )
    {
        fprintf(stderr, "%s: getaddrinfo: %s\n", argv[0], gai_strerror(s));
        exit(1);
    }

    /* Iterate through the address list and try to connect */
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1 )
        {
            continue;
        }

        if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1)
        {
            break;
        }

        close(s);
    }
    if (rp == NULL)
    {
        perror("file_client: connect");
        exit(1);
    }

    /* Send the file name to the server */
    if (serv_send(s, file, strlen(file)+1) == -1) 
    {
        perror("serv_send()");
        close(s);
        exit(1);
    }

    /* Recieve the server's response */
    fcontents = NULL;
    if ((size = serv_recv(s, &fcontents)) == -1)
    {
        perror("serv_recv()");
        close(s);
        exit(1);
    }

    /* Handle error from server */
    if (strcmp(fcontents, FILE_ERR) == 0)
    {
        fprintf(stderr, "Server: Error opening file.\n");
        printf("%s\n", fcontents);
        close(s);
        exit(1);
    }

    /* Write contents to file */
    if (write_file(file, fcontents, size) == -1)
    {
        perror("write_file()");
        close(s);
        exit(1);
    }

    freeaddrinfo(result);
    close(s);

    return 0;
}
