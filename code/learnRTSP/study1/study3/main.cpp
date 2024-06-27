#include <stdio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include "rtp.h"

#pragma comment(lib, "ws2_32.lib")

#define H264_FILE_NAME "../data/test.h264"
#define AAC_FILE_NAME "../data/test-long.aac"

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

static int createUdpSocket()
{
	int sockfd;
	int optVal = 1;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		return -1;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

	return sockfd;

}


static int binSocketAddr(int serverSocketFd, const char* ip, int port) {

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);

	if (bind(serverSocketFd, (struct sockaddr*)&addr, sizeof(struct sockaddr)))
		return -1;

	return 0;

}

static int acceptClient(int serverSocketFd, char* clientIp, int* clientPort)
{
	int clientFd;
	int len = 0;
	struct sockaddr_in clientAddr;

	len = sizeof(struct sockaddr_in);
	memset(&clientAddr, 0, len);


	clientFd = accept(serverSocketFd, (struct sockaddr*)&clientAddr, &len);
	if (clientFd < 0) {
		;
		return -1;
	}

	strcpy(clientIp, inet_ntoa(clientAddr.sin_addr));
	*clientPort = ntohs(clientAddr.sin_port);

	return clientFd;
}


/**
* retval: 0 false; 1 true
*/
static inline int startCode3(char* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
		return 1;
	else
		return 0;
}

/**
* retval: 0 false; 1 true
*/
static inline int startCode4(char* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
		return 1;
	else
		return 0;
}

/**
* 查找h264的下一个NALU
* buf:待检查的内存指针
* len:buf长度
* retval: 返回找到的起始地址
*/
static char* findNextStartCode(char* buf, int len)
{
	int i;
	if (len < 3)
		return NULL;

	for (i = 0; i < len - 1; ++i) {
		if (startCode3(buf) || startCode4(buf)) {
			return buf;
		}
		++buf;
	}

	if (startCode3(buf)) {
		return buf;
	}

	return NULL;
}


/**
* fp: h264文件句柄
* frame: 用来读取数据的buf
* size: 一次性读取多少个字节
* retval: 返回真实的frame大小(此大小保护startCode的大小)
*/
static int getFramFromH264File(FILE* fp, char* frame, int size)
{
	int rSize, frameSize;
	char* nextStartCode;

	if (!fp)
		return -1;

	rSize = fread(frame, 1, size, fp);
	if (rSize <= 0)
		return -1;

	if (!startCode3(frame) && !startCode4(frame))
		return -1;

	nextStartCode = findNextStartCode(frame + 3, rSize - 3);
	if (!nextStartCode) {
		return -1;
	}
	else {
		frameSize = (nextStartCode - frame);
		fseek(fp, frameSize - rSize, SEEK_CUR);
	}

	return frameSize;
}

static int rtpSendH264Frame(int serverRtpSockfd, const char* ip, int16_t port,
	struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize)
{
	uint8_t naluType; //nalu第一个字节
	int sendBytes = 0;
	int ret;

	naluType = frame[0];

	printf("frameSize=%d \n", frameSize);


	if (frameSize <= RTP_MAX_PKT_SIZE) { // nalu长度小于最大包长：单一NALU单元模式
		//*   0 1 2 3 4 5 6 7 8 9
		//*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//*  |F|NRI|  Type   | a single NAL unit ... |
		//*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

		memcpy(rtpPacket->payload, frame, frameSize);
		ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, frameSize);
		if (ret < 0)
			return -1;

		rtpPacket->rtpHeader.seq++;
		sendBytes += ret;
		if ((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8)	//如果是SPS、PPS就不需要加时间戳
			goto out;
	}
	else {	//nalu长度大于最大包长：分片模式
		//*  0                   1                   2
		//*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
		//* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		//* | FU indicator  |   FU header   |   FU payload   ...  |
		//* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+



		//*     FU Indicator
		//*    0 1 2 3 4 5 6 7
		//*   +-+-+-+-+-+-+-+-+
		//*   |F|NRI|  Type   |
		//*   +---------------+



		//*      FU Header
		//*    0 1 2 3 4 5 6 7
		//*   +-+-+-+-+-+-+-+-+
		//*   |S|E|R|  Type   |
		//*   +---------------+

		int pktNum = frameSize / RTP_MAX_PKT_SIZE;			//有几个完整的包
		int remainPktSize = frameSize % RTP_MAX_PKT_SIZE;	//剩余不完整包的大小
		int i, pos = 1;

		//发送完整的包
		for (i = 0; i < pktNum; ++i) {
			rtpPacket->payload[0] = (naluType & 0x60) | 28;	//TODO 疑问:这边naluType & 0x60，为什么不是0xe0(0b1110 0000) 为什么不是取naluType的前3位
			rtpPacket->payload[1] = naluType & 0x1F;

			if (i == 0) //第一包数据
				rtpPacket->payload[1] |= 0x80; //start
			else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
				rtpPacket->payload[1] |= 0x40; //end

			memcpy(rtpPacket->payload + 2, frame + pos, RTP_MAX_PKT_SIZE);

			ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, RTP_MAX_PKT_SIZE + 2);
			if (ret < 0)
				return -1;

			rtpPacket->rtpHeader.seq++;
			sendBytes += ret;
			pos += RTP_MAX_PKT_SIZE;
		}

		//发送剩余数据
		if (remainPktSize > 0) {
			rtpPacket->payload[0] = (naluType & 0x60) | 28;
			rtpPacket->payload[1] = naluType & 0x1F;
			rtpPacket->payload[1] |= 0x40; //end
			memcpy(rtpPacket->payload + 2, frame + pos, remainPktSize + 2); //疑问：最后一个参数应该是不需要+2的

			ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, remainPktSize + 2);
			if (ret < 0)
				return -1;

			rtpPacket->rtpHeader.seq++;
			sendBytes += ret;
		}

	}
	rtpPacket->rtpHeader.timestamp += 90000 / 25;
out:

	return sendBytes;
}

struct AdtsHeader {
	unsigned int syncword;  //12 bit 同步字 '1111 1111 1111'，一个ADTS帧的开始
	uint8_t id;        //1 bit 0代表MPEG-4, 1代表MPEG-2。
	uint8_t layer;     //2 bit 必须为0
	uint8_t protectionAbsent;  //1 bit 1代表没有CRC，0代表有CRC
	uint8_t profile;           //1 bit AAC级别（MPEG-2 AAC中定义了3种profile，MPEG-4 AAC中定义了6种profile）
	uint8_t samplingFreqIndex; //4 bit 采样率
	uint8_t privateBit;        //1bit 编码时设置为0，解码时忽略
	uint8_t channelCfg;        //3 bit 声道数量
	uint8_t originalCopy;      //1bit 编码时设置为0，解码时忽略
	uint8_t home;               //1 bit 编码时设置为0，解码时忽略

	uint8_t copyrightIdentificationBit;   //1 bit 编码时设置为0，解码时忽略
	uint8_t copyrightIdentificationStart; //1 bit 编码时设置为0，解码时忽略
	unsigned int aacFrameLength;               //13 bit 一个ADTS帧的长度包括ADTS头和AAC原始流
	unsigned int adtsBufferFullness;           //11 bit 缓冲区充满度，0x7FF说明是码率可变的码流，不需要此字段。CBR可能需要此字段，不同编码器使用情况不同。这个在使用音频编码的时候需要注意。

	/* number_of_raw_data_blocks_in_frame
	 * 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
	 * 所以说number_of_raw_data_blocks_in_frame == 0
	 * 表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
	 */
	uint8_t numberOfRawDataBlockInFrame; //2 bit
};

static int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) 
{
	static int frame_number = 0;
	memset(res, 0, sizeof(*res));

	if ((in[0] == 0xFF) && ((in[1] & 0xF0) == 0xF0)) 
	{
		res->id = ((uint8_t)in[1] & 0x08) >> 3;//第二个字节与0x08与运算之后，获得第13位bit对应的值
		res->layer = ((uint8_t)in[1] & 0x06) >> 1;//第二个字节与0x06与运算之后，右移1位，获得第14,15位两个bit对应的值
		res->protectionAbsent = (uint8_t)in[1] & 0x01;
		res->profile = ((uint8_t)in[2] & 0xc0) >> 6;
		res->samplingFreqIndex = ((uint8_t)in[2] & 0x3c) >> 2;
		res->privateBit = ((uint8_t)in[2] & 0x02) >> 1;
		res->channelCfg = ((((uint8_t)in[2] & 0x01) << 2) | (((unsigned int)in[3] & 0xc0) >> 6));
		res->originalCopy = ((uint8_t)in[3] & 0x20) >> 5;
		res->home = ((uint8_t)in[3] & 0x10) >> 4;
		res->copyrightIdentificationBit = ((uint8_t)in[3] & 0x08) >> 3;
		res->copyrightIdentificationStart = (uint8_t)in[3] & 0x04 >> 2;

		res->aacFrameLength = (((((unsigned int)in[3]) & 0x03) << 11) |
			(((unsigned int)in[4] & 0xFF) << 3) |
			((unsigned int)in[5] & 0xE0) >> 5);

		res->adtsBufferFullness = (((unsigned int)in[5] & 0x1f) << 6 |
			((unsigned int)in[6] & 0xfc) >> 2);
		res->numberOfRawDataBlockInFrame = ((uint8_t)in[6] & 0x03);

		return 0;
	}
	else {
		printf("failed to parse adts header\n");
		return -1;
	}

}

static int rtpSendAACFrame(int socket, const char* ip, int16_t port,
	struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize)
{
	//打包文档：https://blog.csdn.net/yangguoyu8023/article/details/106517251/
	int ret;

	rtpPacket->payload[0] = 0x00;
	rtpPacket->payload[1] = 0x10;
	rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5;	//高8位
	rtpPacket->payload[3] = (frameSize & 0x1F) << 3;	//低5位

	memcpy(rtpPacket->payload+4, frame, frameSize);

	ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, frameSize+4);
	if (ret < 0) {
		printf("failed to send rtp packet\n");
		return -1;
	}

	rtpPacket->rtpHeader.seq++;

	/**
	* 如果采样率是44100
	* 一般AAC每1024个采样为一帧
	* 所以1秒就有 44100 / 1024 = 43 帧
	* 时间增量就是 44100 / 43 = 1025
	* 一帧的时间为 1 / 43 = 23ms
	*/
	rtpPacket->rtpHeader.timestamp += 1025;

	return 0;
}

int handleCmd_OPTIONS(char* result, int cseq)
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

	sscanf(url, "rtsp://%[^:]:", localIp);

	sprintf(sdp,
		"v=0\r\n"
		"o=- 9%ld 1 IN IP4 %s\r\n"
		"t=0 0\r\n"
		"a=control:*\r\n"
		"m=audio 0 RTP/AVP 97\r\n"
		"a=rtpmap:97 mpeg4-generic/44100/2\r\n"
		"a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
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


static void doClient(int clientSocketFd, char* clientIp, int* clientPort)
{
	char method[40];
	char url[100];
	char version[40];
	int CSeq;

	int serverRtpSockfd = -1, serverRtcpSockfd = -1;
	int clientRtpPort, clienRtcpPort;
	char* rBuf = (char*)malloc(10000);
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
		char* line = strtok(rBuf, sep);
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

			serverRtpSockfd = createUdpSocket();
			serverRtcpSockfd = createUdpSocket();

			if (serverRtpSockfd < 0 || serverRtcpSockfd < 0) {
				printf("failed to create udp socket\n");
				break;
			}

			if (binSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT) < 0 ||
				binSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT < 0)
				) {
				printf("failed to bind addr\n");
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
			int frameSize, startCode;
			struct AdtsHeader adtsHeader;
			uint8_t* frame = (uint8_t*)malloc(5000);
			struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(5000);
			int ret;

			FILE* fp = fopen(AAC_FILE_NAME, "rb");
			if (!fp) {
				printf("读取 %s 失败\n", AAC_FILE_NAME);
				break;
			}
			rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PLAYLOAD_TYPE_AAC, 0, 0, 0, 0x32411);

			printf("start play\n");
			printf("client ip:%s\n", clientIp);
			printf("client port:%d\n", clientRtpPort);

			while (true) {
				ret = fread(frame, 1, 7, fp);
				if (ret <= 0) {
					printf("fread err\n");
					break;
				}
				printf("fread ret=%d \n", ret);

				if (parseAdtsHeader(frame, &adtsHeader) < 0) {
					printf("parseAdtsHeader err\n");
					break;
				}

				ret = fread(frame, 1, adtsHeader.aacFrameLength-7, fp);
				if (ret <= 0) {
					printf("fread err\n");
					break;
				}

				rtpSendAACFrame(serverRtpSockfd, clientIp, clientRtpPort, 
					rtpPacket, frame, adtsHeader.aacFrameLength);

				Sleep(1);
			}

			free(frame);
			free(rtpPacket);
			break;
		}

		memset(method, 0, sizeof(method) / sizeof(char));
		memset(url, 0, sizeof(url) / sizeof(char));
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

	if (listen(serverSocketFd, 10) < 0) {
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