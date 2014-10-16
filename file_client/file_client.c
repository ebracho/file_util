/* file_client.c */

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
            return -1; /* select error */
        }
        if (retval == 0)
        {
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
    struct addrinfo *res;
    char *host, *port, *file;
    char *fcontents;
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

    if ((s = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1 )
    {
        perror("file_client: socket");
        exit(1);
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("file_client: connect");
        close(s);
        exit(1);
    }

    /* Send the file name to the server */
    if (host_send(s, file, strlen(file)+1) == -1) 
    {
        perror("host_send()");
        close(s);
        exit(1);
    }

    /* Recieve the server's response */
    fcontents = NULL;
    if ((size = host_recv(s, &fcontents)) == -1)
    {
        perror("host_recv()");
        close(s);
        exit(1);
    }

    /* Handle error from server */
    if (strcmp(fcontents, FILE_ERR) == 0)
    {
        fprintf(stderr, "Server: Error opening file.\n");
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

    freeaddrinfo(res);
    close(s);

    return 0;
}
