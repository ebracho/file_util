/*
 * A function that randomly drops a packet. packetErrorSend has the same
 * format and arguments as send(2).
 */
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_PACKET_DATA_SIZE (1300)

ssize_t packetErrorSend(int sockfd, const void *buf, size_t len, int flags);
