# 从零编写一个RTSP服务器 笔记

学习 B站 北小菜的《从零编写一个RTSP服务器》的课程， `https://www.bilibili.com/video/BV1xd4y147Fb`

课程内容：

* 第一节：RTSP协议讲解及代码实现
* 第二节：实现一个基于UDP的RTP传输h264的RTSP服务器，并能够进行RTSP拉流播放
* 第三节：实现一个基于UDP的RTP传输aac的RTSP服务器，并能够进行RTSP拉流播放
* 第四节：实现一个基于TCP的RTP 同时传输h264和aac的RTSP服务器，并能够进行rtsp拉流播放
* 第五节：基于开源项目`BXC_RtspServer`进行详细的源码讲解，这是一个完整可用，支持多线程，基于socket编写的IO多路复用的RTSP流媒体服务器

## 第1节 RTSP协议讲解及代码实现

### RTSP协议简介

RTSP是一个实时传输流协议，是一个应用层的协议。通常说的RTSP 包括RTSP协议、RTP协议、RTCP协议，对于这些协议的左右简单的理解如下：

* RTSP协议：负责服务器与客户端之间的请求与相应
* RTP协议：负责服务器与客户端之间传输媒体数据
* RTCP协议：负责提供有关RTP传输质量的反馈，就是确保RTP传输的质量

三者的关系：

* rtp并不会发送媒体数据，只是完成服务器和客户端之间的信令交互，rtp协议负责媒体数据传输，rtcp负责rtp数据包的监视和反馈。
* rtp和rtcp并没有规定传输层的类型，可以选中udp和tcp。
* RTSP的传输层要求是基于tcp。

### 实例：演示RTSP的交互过程

通过一个ffmpeg客户端拉流播放一个rtsp视频流的发送，讲解一下RTSP的交互过程，并脱光Wireshark抓包分析。

用到的工具包括：

* ffmpeg命令行
* wireshark