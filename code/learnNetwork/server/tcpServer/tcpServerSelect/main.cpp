#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <iostream>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")


static std::string getTime() {
	const char* time_fmt = "%Y-%m-%d %H:%M:%S";
	time_t t = time(nullptr);
	char time_str[64];
	strftime(time_str, sizeof(time_str), time_fmt, localtime(&t));

	return time_str;
}

//  __FILE__ ��ȡԴ�ļ������·��������
//  __LINE__ ��ȡ���д������ļ��е��к�
//  __func__ �� __FUNCTION__ ��ȡ������

#define LOGI(format, ...)  fprintf(stderr,"[INFO]%s [%s:%d %s()] " format "\n", getTime().data(),__FILE__,__LINE__,__func__ ,##__VA_ARGS__)
#define LOGE(format, ...)  fprintf(stderr,"[ERROR]%s [%s:%d %s()] " format "\n",getTime().data(),__FILE__,__LINE__,__func__ ,##__VA_ARGS__)


int main() 
{
	const char* ip = "127.0.0.1";
	uint16_t port = 55555;

	LOGI("tcpServerSelect tcp://%s:%d", ip, port);

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		LOGE("WSAStartup Error");
		return -1;
	}

	// ����������
	int serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd < 0) {
		LOGE("create socket error");
		return -1;
	}

	int on = 1;
	setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

	SOCKADDR_IN server_addr;
	server_addr.sin_family = AF_INET; 
	//server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	if (bind(serverFd, (SOCKADDR*)&server_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		LOGE("socket bind error");
		return -1;
	}

	if (listen(serverFd, 10) < 0) {
		LOGE("socket listen error");
		return -1;
	}

	char recvBuf[1000] = { 0 };
	int recvBufLen = 1000;
	int recvLen = 0;

	int maxFd = 0;
	fd_set readFds;
	FD_ZERO(&readFds);

	//��serverFd��ӽ��뼯���ڣ�����������ļ�������
	FD_SET(serverFd, &readFds);
	maxFd = maxFd > serverFd ? maxFd : serverFd;

	struct timeval timeout;
	timeout.tv_sec = 0; //��
	timeout.tv_usec = 0; //΢��

	char sendBuf[100];
	int sendBufLen = 100;
	memset(sendBuf, 0, sendBufLen);

	int ret = 0;
	while (true) {
		fd_set readFdsTemp;
		FD_ZERO(&readFdsTemp);

		readFdsTemp = readFds;

		ret = select(maxFd+1, &readFdsTemp, NULL, NULL, &timeout);
		if (ret < 0) {
			//LOGI("δ��⵽��Ծfd");
		}
		else {
			for (int fd = 3; fd < maxFd + 1; fd++) {
				if (FD_ISSET(fd, &readFdsTemp)) {
					LOGI("fd=%d, �����ɶ��¼�", fd);

					if (fd == serverFd) { // ����׽��־�����ȴ��ͻ�������
						int clientFd;
						if ((clientFd = accept(serverFd, NULL, NULL)) == SOCKET_ERROR) {
							LOGE("accept error");
						}
						LOGI("���������ӣ�clientFd=%d", clientFd);

						// ����пͻ������ӣ����������µ��ļ���������ӵ������У�����������ļ�������
						FD_SET(clientFd, &readFds);
						maxFd = maxFd > clientFd ? maxFd : clientFd;
					}
					else { //�ͻ��˷�����Ϣ
						memset(recvBuf, 0, recvBufLen);

						recvLen = recv(fd, recvBuf, recvBufLen, 0);
						if (recvLen < 0) {
							LOGE("fd=%d,recvLen=%d error", fd, recvLen);
							closesocket(fd);
							FD_CLR(fd, &readFds);	//�ӿɶ�������ɾ��
							continue;
						}
						else {
							LOGI("fd=%d, recLen=%d success", fd, recvLen);
						}

					}
				
				}

			}

#if 0
			for (int i = 0; i < readFds.fd_count; i++) {
				int fd = readFds.fd_array[i];
				if (fd != serverFd) { //�ͻ���fd
					int size = send(fd, sendBuf, sendBufLen, 0);
					if (size < 0) {
						LOGE("fd=%d,send error, ������ %d", fd, WSAGetLastError());
						continue;
					}
					else {
						LOGI("fd=%d,send success lend %d", fd, size);
					}

				}
			
			}
#endif
		}

	}
	
	if (serverFd > -1) {
		closesocket(serverFd);
		serverFd = -1;
	}

	WSACleanup();

	return 0;
}

