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
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#define LINE_LEN 256
#define FILE_ERR "file_err"
#define ERR_LEN strlen(FILE_ERR) + 1

/* Send buffer to sockfd prepended with 4-byte int indicating buffer length */
int host_send(int sockfd, const char *buf, uint32_t len)
{
    char *new_buf = malloc(sizeof(uint32_t) + len);
    memcpy(new_buf, &len, sizeof(uint32_t));
    memcpy(new_buf+sizeof(uint32_t), buf, len);

    int bytes_sent = 1;
    int total = 0;

    while (total < sizeof(uint32_t) + len)
    {
        bytes_sent = send(sockfd, new_buf+total, len-bytes_sent, 0);

        if (bytes_sent == -1)
        {
            return -1; /* recv error */
        }

        total += bytes_sent;
    }

    return 0; /* success */
}

/* Recieve len bytes from sockfd into buf */
int recv_by_len(int sockfd, char *buf, int len)
{
    fd_set readfds;
    struct timeval timeout;

    int retval = 0;
    int total = 0;
    int bytes_recieved = 0;

    while (total < len)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;
        retval = select(sockfd+1, &readfds, NULL, NULL, &timeout);
        if (retval == -1)
        {
            printf("select error\n");
            return -1; /* select error */
        }
        if (retval == 0)
        {
            printf("timed out\n");
            return -1; /* socket timed out */
        }
        bytes_recieved = recv(sockfd, buf+total, len-bytes_recieved, 0);
        if (bytes_recieved == -1)
        {
            return -1; /* recv error */ 
        }
        total += bytes_recieved;
    }

    return 0; /* success */
}

/* Recieve 4-byte int indicating message size, then recieve message into buf */
int host_recv(int sockfd, char **buf)
{
    uint32_t msg_len = 0;

    if (recv_by_len(sockfd, (char*)&msg_len, sizeof(uint32_t)) == -1)
    {
        return -1; /* recv_by_len error */
    }
    *buf = malloc(msg_len);
    if (recv_by_len(sockfd, *buf, msg_len) == -1)
    {
        return -1; /* recv_by_len error */
    }
    return msg_len; /* success */
}


int read_file(char *filename, char **res)
{
    int fd, bytes_read, total;

    if ((fd = open(filename, O_RDONLY, 0)) == -1)
    {
        return -1; /* open error */
    }

    *res = NULL;
    total = 0;
    do
    {
        *res = realloc(*res, total + LINE_LEN);
        if ((bytes_read = read(fd, *res+total, LINE_LEN)) == -1)
        {
            return -1; /* read error */
        }
        total += bytes_read;
    } while (bytes_read != 0);

    close(fd);
    return total;
}

int main(int argc, char *argv[])
{
    struct addrinfo hints;
    struct addrinfo *res;
    char *port, *fcontents, *filename;
    int err, s, new_s, size;
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
    if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1 )
    {
        perror("file_server: socket");
        exit(1);
    }

    if (bind(s, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("file_server: bind");
        close(s);
        exit(1);
    }

    if (listen(s, 1) == -1)
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
    filename = NULL;
    if (host_recv(new_s, &filename) == -1)
    {
        perror("host_recv()");
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
    if (host_send(new_s, fcontents, size) == -1)
    {
        perror("host_send()");
        close(s);
        close(new_s);
        exit(1);
    }

    if (debug) printf("file contents sent\n");

    freeaddrinfo(res);
    close(s);
    close(new_s);

    return 0;
}
