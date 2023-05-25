#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>


#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8554

#define SERVER_RTP_PORT 55532
#define SERVER_RTCP_PORT 55533

static int createTcpSocket()
{
	int sockfd;
	int on = 1;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
	return sockfd;
}

static int bindSocketAddr(int sockfd, const char *ip, int port)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if (bind(sockfd, (SOCKADDR*)&addr, sizeof(addr)) < 0)
		return -1;
	return 0;
}

static int acceptClient(int serverSockfd, char *ip, int *port)
{

	int clientSockfd;
	struct sockaddr_in addr;
	int len = 0;

	len = sizeof(addr);

	memset(&addr, 0, len);
	clientSockfd = accept(serverSockfd, (struct sockaddr *) & addr, &len);
	if (clientSockfd < 0)
		return -1;
	strcpy(ip, inet_ntoa(addr.sin_addr));
	*port = ntohs(addr.sin_port);

	return clientSockfd;
}

static void doClient(int clientSockfd, char *clientIp, int *clientPort)
{
	char method[40];
	char url[100];
	char version[40];
	int CSeq;
	
	int clientRtpPort, clientRtspPort;
	char *rBuf = (char *)malloc(10000);
	char *sBuf = (char *)malloc(10000);

	while (true) {
		int recvLen;

		recvLen = recv(clientSockfd, rBuf, 2000, 0);
		if (recvLen <= 0) {
			break;
		}

		rBuf[recvLen] = '\0';
		std::string recvStr = rBuf;
		printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		printf("%s rBuf = %s \n", __FUNCTION__, rBuf);

		const char *sep = "\n";
		char *line = strtok(rBuf, sep);
		while (line) {
			if (strstr(line, "OPTIONS") ||
				strstr(line, "DESCRIBE") ||
				strstr(line, "SETUP") ||
				strstr(line, "PLAY")
				) {

				if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
					//error
				}
			}
			else if (strstr(line, "CSeq")) {
				if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
					//error
				}
			}
			else if (!strncmp(line, "Transport:", strlen("Transport:"))) {
				// Transport: RTP/AVP/UDP;unicast;client_port=13358-13359
				// Transport: RTP/AVP;unicast;client_port=13358-13359
				if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d", 
					&clientRtpPort, &clientRtspPort) != 2) {
					//error
					printf("parse Transport error \n");
				}
			}
			line = strtok(NULL, sep);
		}

	
	}

	
}

int main(int argc, char *argv[])
{
	WSADATA wsaData;

	// Æô¶¯windows socket start
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("PC Server Socket Start Up Error \n");
		return -1;
	}
	// Æô¶¯windows socket end

	int serverSockfd = createTcpSocket();
	if (serverSockfd) {
		WSACleanup();
		printf("failed to create tcp socket\n");
		return -1;
	}

	if (bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT)) {
		printf("failed bind addr\n");
	}

	if (listen(serverSockfd,10) < 0) {
		printf("falied to listen\n");
		return -1;
	}

	printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);

	while (true) {
		int clientSockfd;
		char clientIp[40];
		int clientPort;

		clientSockfd = acceptClient(serverSockfd, clientIp, &clientPort);
		if (clientSockfd < 0) {
			printf("failed accept client\n");
			return -1;
		}

		doClient(clientSockfd, clientIp, &clientPort);
	}
	closesocket(serverSockfd);

	return 0;
}