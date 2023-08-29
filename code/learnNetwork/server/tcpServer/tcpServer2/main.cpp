#include <WS2tcpip.h>
#include <WinSock2.h>
#include <stdint.h>
#include <iostream>
#include <mutex>
#include <thread>
#include <map>

#pragma comment(lib, "ws2_32.lib")


#define LOGI(format, ...) fprintf(stderr, "[INFO] %s[%d]%s()" format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__ )
#define LOGE(format, ...) fprintf(stderr, "[ERROR] %s[%d]%s()" format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__ )

class Server;

class Connection {

public:
	Connection(Server *server, int clientFd) 
		:m_server(server), m_clientFd(clientFd) {
		LOGI("");
	}

	~Connection() {
		LOGI("");
		closesocket(m_clientFd);
		if (th) {
			th->join();
			delete th;
			th = nullptr;
		}
	}

public:
	typedef void (*DisconnectionCallback)(void* , int );

	int getClientFd(){
		return m_clientFd;
	}

	void setDisconntionCallback(DisconnectionCallback cb, void *arg) {
		m_disconnectionCallback = cb;
		m_arg = arg;
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
				size = send(conn->m_clientFd, buf, sizeof(buf), 0);
				if (size < 0) {
					LOGE("clientFd=%d,send error, ´íÎóÂë:%d\n", conn->m_clientFd, WSAGetLastError());
					// TODO: close connection
					conn->m_disconnectionCallback(conn->m_arg, conn->m_clientFd);
					break;
				}
				totalSize += size;
				if (totalSize > 62914560) //60 MB
				{
					time_t t2 = time(NULL);
					if (t2 - t1 > 0) {
						uint64_t speed = totalSize / 1024 / 1024 / (t2 - t1);
						LOGI("clientFd=%d,size=%d,totalSize=%llu,speed=%llu",
							conn->m_clientFd, size, totalSize, speed);
						totalSize = 0;
						t1 = time(NULL);
					}
				}
			}
			}, this);
	}


private:
	Server *m_server;
	int m_clientFd;
	std::thread *th;
	DisconnectionCallback m_disconnectionCallback = nullptr;
	void* m_arg = nullptr;

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
		if (bind(serverFd, (const struct sockaddr *)&serverAddr, sizeof(struct sockaddr))) {
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
				return -1;
			}

			Connection conn(this, clientFd);
			this->addConnection(&conn);
			conn.setDisconntionCallback(Server::cbDisconnection, this);
			conn.start();
		}
	}

	void handleDisconnection(int clientFd) {
		LOGI("clientFd=%d", clientFd);
		removeConnection(clientFd);
	}

	static void cbDisconnection(void *args, int clientFd) {
		LOGI("clientFd=%d", clientFd);
		Server *server = (Server *)args;
		server->handleDisconnection(clientFd);
	}

private:
	bool addConnection(Connection* conn) {
		m_connMapMtx.lock();

		if (m_connMap.find(conn->getClientFd()) != m_connMap.end()) {
			m_connMapMtx.unlock();
			return false;
		}
		else {
			m_connMap.insert(std::make_pair(conn->getClientFd(), conn));
			m_connMapMtx.unlock();
			return true;
		}
	}

	Connection* getConnection(int clientFd) {
		m_connMapMtx.lock();

		std::map<int, Connection*>::iterator it = m_connMap.find(clientFd);
		if (it != m_connMap.end()) {
			m_connMapMtx.unlock();
			return it->second;
		}
		else {
			m_connMapMtx.unlock();
			return nullptr;
		}
	}

	bool removeConnection(int clientFd) {
		m_connMapMtx.lock();

		auto it = m_connMap.find(clientFd);
		if (it != m_connMap.end()) {
			m_connMapMtx.unlock();
			m_connMap.erase(it);
			return true;
		}
		else {
			return false;
		}
	}


private:
	char *serverIp = nullptr;
	int port = 0;
	std::map<int, Connection*> m_connMap;
	std::mutex m_connMapMtx;

};


int main() {

	return 0;
}