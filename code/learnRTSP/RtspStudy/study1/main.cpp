#include <stdio.h>
#include <winsock2.h>  // windows socket 2 api 头文件 
#include <ws2tcpip.h>  // windows socket 2 TCP/IP 协议套接字的专用头文件

#pragma comment(lib, "ws2_32.lib")


int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed!");
		return 1;
	}

	// 1. 创建socket

	// 2. 绑定socket

	// 3. 监听socket

	// 4. 接收socket
	

	WSACleanup(); // 关闭 Winsock 库
	return 0;
}
