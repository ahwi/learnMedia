# c++ 网络编程

学习资料：

* 视频学习：北小菜  `https://www.bilibili.com/video/BV1Fv4y167kj`

内容概述：

| 课程目录       | 讲解内容                                                     | 运行平台                                              |
| -------------- | ------------------------------------------------------------ | ----------------------------------------------------- |
| 第一讲（本讲） | 基于已经写好的几个demo层层递进的引出IO多路复用，并实现select网络模型的TCP服务器 | 课程中设计的demo，都只兼容windows系统，开发工具vs2019 |
| 第二讲         | 引出poll，epoll网络模型，并实现相应的TCP服务器，之后重点讲一下IO多路复用的概念，以及select/poll/epoll的概念和区别 | 课程中设计的demo，都是兼容linux系统                   |

## 第1讲：IO多路复用select/poll/epoll

### server1: 一次只能处理一个连接

同时只能处理一个连接：

```c++
#include <stdint.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

//  __FILE__ 获取源文件的相对路径和名字
//  __LINE__ 获取该行代码在文件中的行号
//  __func__ 或 __FUNCTION__ 获取函数名
#define LOGI(format, ...)  fprintf(stderr,"[INFO] [%s:%d]:%s() " format "\n", __FILE__,__LINE__,__func__,##__VA_ARGS__)
#define LOGE(format, ...)  fprintf(stderr,"[ERROR] [%s:%d]:%s() " format "\n", __FILE__,__LINE__,__func__,##__VA_ARGS__)


int main() {

	const char* ip = "127.0.0.1";
	uint16_t port = 8080;
	LOGI("TcpServer1 tcp://%s:%d", ip, port);

	SOCKET server_fd = -1;
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		LOGI("WSAStartup error");
		return -1;
	}
	SOCKADDR_IN server_addr;

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	//server_addr.sin_addr.s_addr = inet_addr("192.168.2.61");
	server_addr.sin_port = htons(port);

	server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (bind(server_fd, (SOCKADDR*)&server_addr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		LOGI("socket bind error");
		return -1;
	}

	if (listen(server_fd, SOMAXCONN) < 0) {
		LOGI("socket listen error");
		return -1;
	}

	while (true)
	{
		LOGI("阻塞监听新连接...");
		// 阻塞接收请求 start
		int len = sizeof(SOCKADDR);
		SOCKADDR_IN accept_addr;
		int clientFd = accept(server_fd, (SOCKADDR*)&accept_addr, &len);
		//const char* clientIp = inet_ntoa(accept_addr.sin_addr);

		if (clientFd == SOCKET_ERROR) {
			LOGI("accept connection error");
			break;
		}
		// 阻塞接收请求 end
		LOGI("发现新连接：clientFd=%d", clientFd);

		int size;
		uint64_t totalSize = 0;
		time_t  t1 = time(NULL);

		while (true) {
			char buf[1024];
			memset(buf, 1, sizeof(buf));
			size = ::send(clientFd, buf, sizeof(buf), 0);
			if (size < 0) {
				printf("clientFd=%d,send error，错误码：%d\n", clientFd, WSAGetLastError());

				break;
			}
			totalSize += size;

			if (totalSize > 62914560)/* 62914560=60*1024*1024=60mb*/
			{
				time_t t2 = time(NULL);
				if (t2 - t1 > 0) {
					uint64_t speed = totalSize / 1024 / 1024 / (t2 - t1);
					printf("clientFd=%d,size=%d,totalSize=%llu,speed=%lluMbps\n", clientFd, size, totalSize, speed);

					totalSize = 0;
					t1 = time(NULL);
				}
			}


		}

		LOGI("关闭连接 clientFd=%d", clientFd);
	}

	return 0;
}
```



### server2：用多线程处理连接

如果有新连接，就开一个新的线程去处理连接。

**缺点：**

无论是linux还是windows，一个进程可用的内存空间都有上限，比如windows内存空间上限有2G，
而默认情况下，一个线程的栈要预留1M的内存空间，所以理论一个进程中最多可以开2048个线程
但实际上远远达不到。

虽然能实现一对n的服务，但很显然不适合高并发场景。



代码：

```c++
while (true) {
    LOGI("阻塞监听新连接...");
    // 阻塞接收请求 start
    int clientFd;
    char clientIp[40] = { 0 };
    uint16_t clientPort;

    socklen_t len = 0;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);

    clientFd = accept(m_sockFd, (struct sockaddr*)&addr, &len);
    if (clientFd < 0)
    {
        LOGE("socket accept error");
        return -1;
    }
    strcpy(clientIp, inet_ntoa(addr.sin_addr));
    clientPort = ntohs(addr.sin_port);
    // 阻塞接收请求 end
    LOGI("发现新连接：clientIp=%s,clientPort=%d,clientFd=%d", clientIp, clientPort, clientFd);


    Connection* conn = new Connection(this, clientFd);
    conn->setDisconnectionCallback(Server::cbDisconnection, this);
    this->addConnection(conn);
    conn->start();//非阻塞在子线程中启动

}
```

有新连接时，就创建一个Connection线程来处理连接。

详细代码参考：`TcpServer2`





