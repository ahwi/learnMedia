#include <stdio.h>
#include <winsock2.h>  // windows socket 2 api ͷ�ļ� 
#include <ws2tcpip.h>  // windows socket 2 TCP/IP Э���׽��ֵ�ר��ͷ�ļ�

#pragma comment(lib, "ws2_32.lib")


SOCKET CreateSocket() 
{
	SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd <= 0) {
		printf("create socket error!\n");
		return -1;
	}

	// ����socket ��������
	bool reuseaddr = TRUE;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr, sizeof(reuseaddr));

	return sockfd;
}

int Bind(SOCKET sock, const char *ip, const int& port)
{
	struct sockaddr_in  addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	if(bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)))
		return -1;

	return 0;
}

SOCKET AcceptSocket(SOCKET serverSock) 
{
	sockaddr clientAddr;
	memset(&clientAddr, 0, sizeof(clientAddr));
	int len = sizeof(clientAddr);

	SOCKET clientfd = accept(serverSock, &clientAddr, &len);
	if (clientfd < 0) {
		return -1;
	}

	return clientfd;
}

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed!\n");
		return -1;
	}

	// 1. ����socket
	SOCKET sockfd = CreateSocket();
	if (sockfd < 0) {
		WSACleanup();
		printf("create server socket error!\n");
		return -1;
	}

	// 2. ��socket
	int port = 27015;
	if (Bind(sockfd, "0.0.0.0", port)) {
		WSACleanup();
		printf("bind socket error!\n");
		return -1;
	}

	// 3. ����socket
	if (listen(sockfd, 10) < 0) {
		WSACleanup();
		printf("listen error!\n");
		perror("listen");
		return -1;
	}

	// 4. ����socket
	SOCKET clientSock = AcceptSocket(sockfd);
	if (clientSock < 0 ) {
		WSACleanup();
		printf("accept socket error!\n");
		return -1;
	}
	

	WSACleanup(); // �ر� Winsock ��
	return 0;
}
