#include <stdio.h>
#include <winsock2.h>  // windows socket 2 api ͷ�ļ� 
#include <ws2tcpip.h>  // windows socket 2 TCP/IP Э���׽��ֵ�ר��ͷ�ļ�

#pragma comment(lib, "ws2_32.lib")


int main(int argc, char *argv[])
{
	WSADATA wsaData;
	int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != 0) {
		printf("WSAStartup failed!");
		return 1;
	}

	// 1. ����socket

	// 2. ��socket

	// 3. ����socket

	// 4. ����socket
	

	WSACleanup(); // �ر� Winsock ��
	return 0;
}
