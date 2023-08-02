#include <stdio.h>
#include <winsock2.h>  // windows socket 2 api 头文件 
#include <ws2tcpip.h>  // windows socket 2 TCP/IP 协议套接字的专用头文件
#include <time.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8554
#define SERVER_RTP_PORT 55532
#define SERVER_RTCP_PORT 55533
#define SESSION 66334873

int createTcpSocket() 
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd <= 0) {
		printf("create socket error!\n");
		return -1;
	}

	// 设置socket 可以重用
	bool reuseaddr = TRUE;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr, sizeof(reuseaddr));

	return sockfd;
}

int bindSocketAddr(int sock, const char *ip, const int& port)
{
	struct sockaddr_in  addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);

	if(bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr)))
		return -1;

	return 0;
}

int acceptClient(int serverSock, char *ip, int *port) 
{
	sockaddr_in clientAddr;
	memset(&clientAddr, 0, sizeof(clientAddr));
	int len = sizeof(clientAddr);

	int clientfd = accept(serverSock, (struct sockaddr *) &clientAddr, &len);
	if (clientfd < 0) {
		return -1;
	}
	strcpy(ip,inet_ntoa(clientAddr.sin_addr));
	*port = ntohs(clientAddr.sin_port);

	return clientfd;
}

static int handleCmd_OPTIONS(char *sendBuf, const int &CSeq) 
{
	sprintf(sendBuf, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq:%d\r\n"
		"Public:OPTIONS,DESCRIBE,SETUP,PLAY\r\n"
		"\r\n", CSeq
	);
	return 0;
}

static int handleCmd_DESCRIBE(char *sendBuf, const int &CSeq, const char *url) 
{
	char ip[40];
	sscanf(url, "rtsp://%[^:]:", ip);

	char sdp[400];
	sprintf(sdp, 
		"v=0\r\n"
		"o=- 9%ld 1 IN IP4 %s\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=video 0 RTP/AVP 96\r\n"
		"a=rtmap:96 H264/90000\r\n"
		"a=control:track0\r\n", time(NULL), ip
	);


	sprintf(sendBuf, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq:%d\r\n"
		"Content-Base:%s\r\n"
		"Content-type:application/sdp"
		"Content-lenth:%zu\r\n\r\n"
		"%s",
		CSeq,
		url,
		strlen(sdp),
		sdp
	);
	return 0;
}

static int handleCmd_SETUP(char *sendBuf, const int &CSeq, const int &clientRtpPort) 
{
	sprintf(sendBuf, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq:%d\r\n"
		"Transport:RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
		"Session:%d\r\n"
		"\r\n",
		CSeq,
		clientRtpPort,
		clientRtpPort + 1,
		SERVER_RTP_PORT,
		SERVER_RTCP_PORT,
		SESSION
		);
	return 0;
}

static int handleCmd_PLAY(char *sendBuf, const int &CSeq)
{
	sprintf(sendBuf,
		"RTSP/1.0 200 OK\r\n"
		"CSeq:%d\r\n"
		"Range:npt=0.000-\r\n"
		"Session:%d;timeout=10\r\n"
		"\r\n",
		CSeq,
		SESSION
		);
	return 0;

}

void doClient(int clientSockFd, const char *clientIp, const int &clientPort) 
{
	char method[40];
	char url[200];
	char version[40];

	int clientRtpPort = 0, clientRtspPort = 0;
	char *recvBuf = (char *)malloc(10000);
	char *sendBuf = (char *)malloc(10000);
	int CSeq = 0;

	while (true) {
		int recvLen = 0;
		recvLen = recv(clientSockFd, recvBuf, 2000, 0);
		if (recvLen < 0)
			break;

		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		printf("%s rBuf = %s \n", __FUNCTION__, recvBuf);

		recvBuf[recvLen] = '\0';
		const char *sep = "\n";
		char * line = strtok(recvBuf, sep);
		while (line) {
			if (strstr(line, "OPTIONS") ||
				strstr(line, "DESCRIBE") ||
				strstr(line, "SETUP") ||
				strstr(line, "PLAY")) {

				if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
					// error
				}
			}
			else if (strstr(line, "CSeq")) {
				if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
					// error
				}
			}
			else if (!strncmp(line, "Transport:", strlen("Transport:"))) {
				if (sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n\r\n", 
					&clientRtpPort, &clientRtspPort) != 1) {
					// error
					printf("parse Transport error \n");
				}
			}
			line = strtok(NULL, sep);
		}

		if (!strcmp(method, "OPTIONS")) {
			if (handleCmd_OPTIONS(sendBuf, CSeq)) {
				printf("failed to handle options \n");
				break;
			}
		}
		else if (!strcmp(method, "DESCRIBE")) {
		
			if (handleCmd_DESCRIBE(sendBuf, CSeq, url)) {
				printf("failed to handle describe \n");
				break;
			}
		}
		else if (!strcmp(method, "SETUP")) {
			if (handleCmd_SETUP(sendBuf, CSeq, clientRtpPort)) {
				printf("failed to handle setup \n");
				break;
			}
		}
		else if (!strcmp(method, "PLAY")) {
			if (handleCmd_PLAY(sendBuf, CSeq)) {
				printf("failed to handle play \n");
				break;
			}
		}
		else {
			printf("未定义的method = %s \n", method);
			break;
		}
		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		printf("%s sBuf = %s \n", __FUNCTION__, sendBuf);


		send(clientSockFd, sendBuf, strlen(sendBuf), 0);

		//开始播放，发送RTP包
		if (!strcmp(method, "PLAY")) {

			printf("start play\n");
			printf("client ip:%s\n", clientIp);
			printf("client port:%d\n", clientPort);
			while (true) {
				Sleep(40);
			}
			break;
		
		}

		memset(method, 0, sizeof(method));
		memset(url, 0, sizeof(url));
		memset(version, 0, sizeof(version));
		CSeq = 0;
	
	}

	closesocket(clientSockFd);
	free(recvBuf);
	free(sendBuf);
}

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed!\n");
		return -1;
	}

	// 1. 创建socket
	int sockfd = createTcpSocket();
	if (sockfd < 0) {
		WSACleanup();
		printf("create server socket error!\n");
		return -1;
	}

	// 2. 绑定socket
	if (bindSocketAddr(sockfd, "0.0.0.0", SERVER_PORT)) {
		WSACleanup();
		printf("bind socket error!\n");
		return -1;
	}

	// 3. 监听socket
	if (listen(sockfd, 10) < 0) {
		WSACleanup();
		printf("listen error!\n");
		perror("listen");
		return -1;
	}

	printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);

	while (true) {
		// 4. 接受socket
		char clientIp[40];
		int clientPort;
		int clientSock = acceptClient(sockfd, clientIp, &clientPort);
		if (clientSock < 0 ) {
			WSACleanup();
			printf("accept socket error!\n");
			return -1;
		}
		printf("accpet client:%s %d\n", clientIp, clientPort);
		doClient(clientSock, clientIp, clientPort);
	}
	
	WSACleanup(); // 关闭 Winsock 库
	return 0;
}
