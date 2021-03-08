#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

void msg_process(int sockfd)
{
    int numbytes = 0, connect_fd = 0;
    struct sockaddr_in client_addr;
    socklen_t sin_size;
    char buf[1024] = {0};
    memset(&client_addr, 0, sizeof(client_addr));
    printf("msg_process started!\n");
	sin_size = sizeof(struct sockaddr_in);
    do {
        connect_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size);
    } while(connect_fd == -1);
	printf("server: got connection\n");
	while (1) {
		if ((numbytes = recv(connect_fd, buf, 1024, 0)) == -1) {
			perror("recv error");
			return;
		}
		if (numbytes) {
			buf[numbytes] = '\0';
			printf("server received: %s\n", buf);
			if (strstr(buf, "quit")) {
				break;
			}
			sleep(1);
		}
	}
    close(connect_fd);
	close(sockfd);
}

void *remote_process(void *arg)
{
	int sockfd;
    struct sockaddr_in server_addr;
	while (1) {
#ifdef USE_SOCK_CLOEXEC
		if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)) == -1) {
#else
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
#endif
			perror("socket error");
			return NULL;
		}
#ifdef USE_FD_CLOEXEC
        fcntl(sockfd, F_SETFD, FD_CLOEXEC);
#endif
		memset(&server_addr, 0, sizeof(server_addr));
		
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(1234);
		server_addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1) {
			perror("bind error");
			return NULL;
		}
		if (listen(sockfd, 10) == -1) {
			perror("listen error");
			return NULL;
		}
		msg_process(sockfd);
	}

    return NULL;
}

void *sub_process(void *arg)
{
	pid_t pid;
	pid = fork();
	if (pid > 0) {
		printf("test: test_fork end\n");
	} else if (pid == 0) {
		execl("/bin/sh", "sh", "-c", "./subprocess", NULL);
		_exit(127);
	} else {
		perror("fork failed\n");
	}
	return NULL;
}

int main()
{
	pthread_t tid1, tid2;
	printf("test: remote_process started!\n");
    pthread_create(&tid1, NULL, remote_process, NULL);
	sleep(5);
	printf("test: sub_process start!\n");
	pthread_create(&tid2, NULL, sub_process, NULL);
	pthread_join(tid2, NULL);
    pthread_join(tid1, NULL);
	return 0;
}
