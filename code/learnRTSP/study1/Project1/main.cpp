#include <stdio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8554 
#define SERVER_RTP_PORT		55532
#define SERVER_RTCP_PORT	55533



static int createTcpSocket()
{
	int sockfd;
	int optVal = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		return -1;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

	return sockfd;
}

static int binSocketAddr(int serverSocketFd, const char *ip, int port) {

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if(bind(serverSocketFd, (struct sockaddr *) &addr, sizeof(struct sockaddr)))
		return -1;

	return 0;

}

static int acceptClient(int serverSocketFd, char *clientIp, int *clientPort)
{
	int clientFd;
	int len = 0;
	struct sockaddr_in clientAddr;

	len = sizeof(struct sockaddr_in);
	memset(&clientAddr, 0, len);
	

	clientFd = accept(serverSocketFd, (struct sockaddr*)&clientAddr, &len);
	if (clientFd < 0) {;
		return -1;
	}

	strcpy(clientIp, inet_ntoa(clientAddr.sin_addr));
	*clientPort = ntohs(clientAddr.sin_port);

	return clientFd;
}

int handleCmd_OPTIONS(char *result, int cseq) 
{
	sprintf(result,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
		"\r\n",
		cseq);

	return 0;
}


int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{
	char sdp[500];
	char localIp[100];

	sscanf(url,  "rtsp://%[^:]:", localIp);

	sprintf(sdp, 
		"v=0\r\n"
		"o=- 9%ld 1 IN IP4 %s\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=video 0 RTP/AVP 96\r\n"
		"a=rtpmap:96 H264/90000\r\n"
		"a=control:track0\r\n",
		time(NULL), localIp
	);

	sprintf(result, 
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Content-Base:%s\r\n"
		"Content-type:application/sdp\r\n"
		"Content-length: %zu\r\n"
		"\r\n"
		"%s",
		cseq, 
		url,
		strlen(sdp),
		sdp
		);

	return 0;
}


int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
	sprintf(result,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
		"Session:66334873\r\n"
		"\r\n",
		cseq, 
		clientRtpPort, 
		clientRtpPort + 1, 
		SERVER_RTP_PORT,
		SERVER_RTCP_PORT
		);

	return 0;
}



int handleCmd_PLAY(char* result, int cseq)
{
	sprintf(result,
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Range: ntp=0.000-\r\n"
		"Session: 66334873; timeout=10\r\n\r\n",
		cseq
		);

	return 0;
}


static void doClient(int clientSocketFd, char *clientIp, int *clientPort) 
{
	char method[40];
	char url[100];
	char version[40];
	int CSeq;

	int clientRtpPort, clienRtcpPort;
	char* rBuf = (char *)malloc(10000);
	char* sBuf = (char*)malloc(10000);

	while (TRUE) {
		int recvLen;

		// 接收数据到rBuf中
		recvLen = recv(clientSocketFd, rBuf, 2000, 0);
		if (recvLen <= 0)
			break;
		rBuf[recvLen] = '\0';

		std::string recvStr = rBuf;
		printf(">>>>>>>>>>>>>>>>>>>>>>>>\n");
		printf("%s rBuf = %s\n", __FUNCTION__, rBuf);

		//解析数据
		const char* sep = "\n";
		char * line = strtok(rBuf, sep);
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
				if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n", 
					&clientRtpPort, &clienRtcpPort) != 2) {
					// error
					printf("parse Transport error\n");
				}

			}
			line = strtok(NULL, sep);
		}

		if (!strcmp(method, "OPTIONS")) {
			if (handleCmd_OPTIONS(sBuf, CSeq)) {
				printf("failed to handle options\n");
				break;
			}
		}
		else if (!strcmp(method, "DESCRIBE")) {
			if (handleCmd_DESCRIBE(sBuf, CSeq, url)) {
				printf("failed to handle describe\n");
				break;
			}
		}
		else if (!strcmp(method, "SETUP")) {
			if (handleCmd_SETUP(sBuf, CSeq, clientRtpPort)) {
				printf("failed to handle setup\n");
				break;
			}
		}
		else if (!strcmp(method, "PLAY")) {
			if (handleCmd_PLAY(sBuf, CSeq)) {
				printf("failed to handle play\n");
				break;
			}
		}
		else {
			printf("未定义的method = %s \n", method);
		}
		printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		printf("%s sBuf = %s \n", __FUNCTION__, sBuf);

		send(clientSocketFd, sBuf, strlen(sBuf), 0);

		//开始播放，发送RTP包
		if (!strcmp(method, "PLAY")) {
			printf("start play\n");
			printf("client ip:%s\n", clientIp);
			printf("client port:%d\n", clientRtpPort);

			while (true) {
				Sleep(40);
				//usleep(40000);	// 1000/25 * 1000
			}
			break;
		}

		memset(method, 0, sizeof(method)/sizeof(char));
		memset(url, 0, sizeof(url)/sizeof(char));
		CSeq = 0;
	}

	closesocket(clientSocketFd);
	free(rBuf);
	free(sBuf);
}

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("PC Server Socket start error!\n");
		return -1;
	}

	int serverSocketFd;
	serverSocketFd = createTcpSocket();
	if (serverSocketFd < 0) {
		WSACleanup();
		printf("create socket error!\n");
		return -1;
	}

	if (binSocketAddr(serverSocketFd, "0.0.0.0", SERVER_PORT) < 0)
	{
		printf("failed to bind addr\n");
		return -1;
	}

	if (listen(serverSocketFd, 10) <0) {
		printf("failed to listen\n");
		return -1;
	}

	printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);

	while (TRUE) {
		int clientSocketFd;
		char clientIp[40];
		int clientPort;

		clientSocketFd = acceptClient(serverSocketFd, clientIp, &clientPort);
		if (clientSocketFd < 0)
		{
			printf("failed to accept client\n");
			return -1;
		}

		printf("accept client:client ip:%s, client port:%d\n", clientIp, clientPort);

		doClient(clientSocketFd, clientIp, &clientPort);
	}

	closesocket(serverSocketFd);
	return 0;
}