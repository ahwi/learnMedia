#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdint.h>
#include <iostream>
#include <mutex>
#include <thread>

#pragma comment(lib, "ws2_32.lib")


#define LOGI(format, ...) fprintf(stderr, "[INFO] %s[%d]%s()" format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__ )
#define LOGE(format, ...) fprintf(stderr, "[ERROR] %s[%d]%s()" format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__ )

class Server;

class Connection {

public:
	Connection(Server *server, int clientFd) 
		:m_server(server), clientFd(clientFd) {
	}

	~Connection() {
	
	}

	int start() {
		th = new std::thread([](Connection* conn) {
			int size = 0;
			uint64_t totalSize = 0;
			char buf[1024];
			memset(buf, 0, sizeof(buf));
			time_t t1 = time(NULL);
			while (true)
			{
				size = send(conn->clientFd, buf, sizeof(buf), 0);
				if (size < 0) {
					LOGE("clientFd=%d,send error, ´íÎóÂë:%d\n", conn->clientFd, WSAGetLastError());
					// TODO: close connection
					break;
				}
				totalSize += size;
				if (totalSize > 62914560) //60 MB
				{
					time_t t2 = time(NULL);
					if (t2 - t1 > 0) {
						uint64_t speed = totalSize / 1024 / 1024 / (t2 - t1);
						LOGI("clientFd=%d,size=%d,totalSize=%llu,speed=%llu",
							conn->clientFd, size, totalSize, speed);
						totalSize = 0;
						t1 = time(NULL);
					}
				}
			}
			}, this);
	}


private:
	Server *m_server;
	int clientFd;
	std::thread *th;

};

class Server {
public:
	Server(char *serverIp, int port) : serverIp(serverIp), port(port)
	{
	}

	int start() {
		struct WSAData wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
			LOGE("WSAStartup error");
			return -1;
		}

		int serverFd = socket(AF_INET, SOCK_STREAM, 0);
		if (serverFd < 0) {
			LOGE("create socket error");
			return -1;
		}
		sockaddr_in serverAddr;
		serverAddr.sin_addr.s_addr = inet_addr(serverIp);
		if (bind(serverFd, (const struct sockaddr *)serverAddr, sizeof(struct sockaddr))) {
			LOGE("bind socket error");
			return -1;
		}

		if (listen(serverFd, 4) < 0) {
			LOGE("listen error");
			return -1;
		}

		while (true) {
			sockaddr_in serverAddr;
			int len;
			int clientFd = accept(serverFd, (sockaddr *)&serverAddr, &len);
			if (clientFd < 0) {
				LOGE("accept error");
				return;
			}

			Connection conn(this, clientFd);
			conn.start();

		
		}

		
	
	}

private:
	char *serverIp = nullptr;
	int port = 0;

};


int main() {

	return 0;
}