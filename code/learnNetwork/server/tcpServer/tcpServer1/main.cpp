#include <stdint.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")


// __FILE__ ��ȡԴ�ļ������·��������
// __LINE__ ��ȡ���д������ļ��е��к�
// __func__ �� __FUNCTION__ ��ȡ������
#define LOGI(format, ...) fprintf(stderr, "[INFO] [%s:%d]:%s() " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__) 
#define LOGE(format, ...) fprintf(stderr, "[ERROR] [%s:%d]:%s() " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__) 

int main() {
    const char *ip = "127.0.0.1";
    uint16_t port = 8080;
    printf("#########");
    LOGI("TcpServer1 tcp://%s:%d", ip, port);

    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        LOGE("WSAStartup error");
        return -1;
    }

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        LOGE("create socket error");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    // addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    if (bind(serverFd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        LOGE("bind socket error");
        return -1;
    }

    if (listen(serverFd, 4) < 0) {
        LOGE("listen socket error");
        return -1;
    }

    while (true) {
        LOGI("��������������...");
        struct sockaddr_in clientAddr;
        int len = sizeof(clientAddr);
        int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &len);

        if (clientFd < 0) {
            LOGE("accept socket error");
            return -1;

        }
        LOGI("����������: clientFd=%d", clientFd);

        int size = 0;
        uint64_t totalSize = 0;
        time_t t1 = time(NULL);

        while (true) {
            char buf[1024];
            memset(buf, 1, sizeof(buf));
            size = send(clientFd, buf, sizeof(buf), 0);
            if (size < 0) {
                printf("clientFd=%d, send error,������:%d\n", clientAddr, WSAGetLastError());
                break;
            }
            totalSize += size;
            if (totalSize > 62914560) { // 60 MB
                time_t t2 = time(NULL);
                if (t2 - t1 > 0) {
                    uint64_t speed = totalSize / 1024 / 1024 / (t2 - t1);
                    printf("clientFd=%d,size=%d,totalSize=%llu,speed=%lluMbps\n", clientFd, size, totalSize, speed);

                    totalSize = 0;
                    t1 = time(NULL);
                }
            }

        }
        LOGI("�ر����� clientFd=%d", clientFd);
    }

    return 0;
}