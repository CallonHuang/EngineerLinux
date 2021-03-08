#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    int sockfd, numbytes, i, ret;
    struct sockaddr_in server_addr;
    char buf[3][1024] = {"Hello, world!", "quit!", "Hello, world! Again!"};
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        return 1;
    }
    memset(&server_addr, 0, sizeof(struct sockaddr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    do {
        ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
    } while(ret == -1);
    for (i = 0; i < 3; i++) {
        printf("send: %s\n", buf[i]);
        if (send(sockfd, buf[i], sizeof(buf[i]), 0) == -1) {
            perror("send error");
            return 1;
        }
        sleep(5);
    }
    close(sockfd);
    return 0;
}
