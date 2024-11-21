#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

#define PORT 55555


int main()
{
    int ret = 0;

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd == -1) {
        perror("socket create failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(serverFd, (const sockaddr *)&addr, sizeof(struct sockaddr_in));
    if(ret == -1) {
        perror("socket bind failed");
        exit(EXIT_FAILURE);
    }

    ret = listen(serverFd, 10);
    if(ret == -1) {
        perror("socket list failed");
        exit(EXIT_FAILURE);
    }

    struct pollfd pollFd[1024];
    for(int i=0; i < 1024; i++){
        pollFd[i].fd = -1;
        pollFd[i].events = POLLIN;
    }

    pollFd[0].fd = serverFd;
    pollFd[0].events = POLLIN;

    char recvBuf[1000] = {0};
    int32_t recvBufLen = 1000;
    memset(recvBuf, 0, recvBufLen);

    char sendBuf[10000] = {0};
    int32_t sendBufLen = 10000;
    memset(sendBuf, 0, sendBufLen);

    nfds_t maxFd = 0;
    while(true) {

        ret = poll(pollFd, maxFd+1, 1);
        if(ret == -1) {
            perror("poll error");
            exit(EXIT_FAILURE);
        }

        /* 服务端socket有读事件，接收新的连接请求 */ 
        if(pollFd[0].revents & POLLIN) {
            int clientSocket = accept(serverFd, NULL, NULL);
            if(clientSocket == -1) {
                perror("accept error");
            } else {
                for(int i=0; i<1024; i++) {
                    if(pollFd[i].fd == -1) {
                        pollFd[i].fd = clientSocket;
                        pollFd[i].events = POLLIN;
                        maxFd = maxFd > i ? maxFd : i;
                        break;
                    }
                }

            }
        }

        for(int i=1; i < maxFd+1; i++) {
            memset(recvBuf, 0, recvBufLen);

            /* 接收数据 */
            if(pollFd[i].revents & POLLIN) {
                ssize_t size = recv(pollFd[i].fd, recvBuf, recvBufLen, 0);
                if(size > 0) {
                    printf("client %d recv size %ld\n", pollFd[i].fd, size);
                }
                else {
                    printf("client %d disconnect\n", pollFd[i].fd);
                    close(pollFd[i].fd);
                    pollFd[i].fd = -1;
                    continue;
                }

            }

            if(pollFd[i].fd != -1) {
                ssize_t size = send(pollFd[i].fd, sendBuf, sendBufLen, 0);
                if(size > 0) {
                    printf("client %d send size %ld\n", pollFd[i].fd, size);
                }
                else {
                    printf("client %d send error\n", pollFd[i].fd);
                    close(pollFd[i].fd);
                    pollFd[i].fd = -1;
                }
            }

        }
        
    }

    close(serverFd);
    

}
