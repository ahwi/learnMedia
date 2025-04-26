# 从零编写一个RTSP服务器 笔记

学习 B站 北小菜的《从零编写一个RTSP服务器》的课程， `https://www.bilibili.com/video/BV1xd4y147Fb`

示例代码：`https://gitee.com/Vanishi/BXC_RtspServer_study`

课程内容：

* 第一节：RTSP协议讲解及代码实现
* 第二节：实现一个基于UDP的RTP传输h264的RTSP服务器，并能够进行RTSP拉流播放
* 第三节：实现一个基于UDP的RTP传输aac的RTSP服务器，并能够进行RTSP拉流播放
* 第四节：实现一个基于TCP的RTP 同时传输h264和aac的RTSP服务器，并能够进行rtsp拉流播放
* 第五节：基于开源项目`BXC_RtspServer`进行详细的源码讲解，这是一个完整可用，支持多线程，基于socket编写的IO多路复用的RTSP流媒体服务器

## 第1节 RTSP协议讲解及代码实现

### RTSP协议简介

RTSP是一个实时传输流协议，是一个应用层的协议。通常说的RTSP 包括RTSP协议、RTP协议、RTCP协议，对于这些协议的左右简单的理解如下：

* RTSP协议：负责服务器与客户端之间的请求与响应
* RTP协议：负责服务器与客户端之间传输媒体数据
* RTCP协议：负责提供有关RTP传输质量的反馈，就是确保RTP传输的质量

三者的关系：

* rstp并不会发送媒体数据，只是完成服务器和客户端之间的信令交互，rtp协议负责媒体数据传输，rtcp负责rtp数据包的监视和反馈。
* rtp和rtcp并没有规定传输层的类型，可以选择udp或tcp。
* RTSP的传输层要求是基于tcp。

### 实例：演示RTSP的交互过程

通过一个ffmpeg客户端拉流播放一个rtsp视频流的发送，讲解一下RTSP的交互过程，并通过Wireshark抓包分析。

用到的工具包括：

* ffmpeg命令行
* wireshark

示例代码：`study1`

#### 抓包分析流程

1. 运行服务端代码：

   ![image-20230517233826895](从零编写一个RTSP服务器.assets/image-20230517233826895.png)

2. 运行wireshark抓包软件

3. 运行 ffplay （当作客户端使用）：

   `ffplay -i rtsp://127.0.0.1:8554`

抓到的包

![image-20230517234012023](从零编写一个RTSP服务器.assets/image-20230517234012023.png)

**rtsp流程分析：**

**1. `OPTIONS` 请求**

<font color=red>客户端向服务端请求数据时，首先发送`OPTIONS`请求。</font>

客户端在请求视频流之前，需要知道服务端支持哪些rtsp类型的方法，所以使用该请求获取相关信息。

![image-20230518220519747](从零编写一个RTSP服务器.assets/image-20230518220519747.png)

* RTSP是基于tcp的，所以传输层使用tcp协议

* 该RTSP数据包的方法是`OPTIONS`

服务器返回的数据包

![image-20230518220933397](从零编写一个RTSP服务器.assets/image-20230518220933397.png)

* 200表示请求成功
* Public：表示支持的方法，该服务端支持的方法有：
  * `OPTIONS` 
  * `DESCRIBE` ：流媒体数据的描述，如视频流的格式或编码等
  * `SETUP`：建立连接
  * `PLAY`：连接建立完后，通过`SETUP`建立的通道，源源不断的获取数据

**2. `DESCRIBE`**

![image-20230518221546636](从零编写一个RTSP服务器.assets/image-20230518221546636.png)

返回的数据包

![image-20230518221655870](从零编写一个RTSP服务器.assets/image-20230518221655870.png)

<font color=blue>SDP协议（会话描述协议）</font>

包括：

* 一个会话级描述
* 多个媒体级描述

简单介绍的一个文档：`https://blog.csdn.net/uianster/article/details/125902301`

> 客户端向服务器请求一个`DESCRIBE`时，服务器返回一个SDP数据时，会包含一个会话级描述和多个媒体级描述。
>
> 会话级描述：包含两者的ip、端口等一些建立连接的基本描述。
>
> 多个媒体级描述：如当前请求的流媒体资源包含音频和视频，那就是有两种流，两种流就有两个媒体级描述。

<font color=blue>SDP数据包字段的解释</font>

> 参考上面的博客文章，对应抓到的数据包

* media

  ```txt
  # media 必须
  m=<media><port><transport><fmt/payload type list>
  例子：m=audio 9 UDP/TLS/RTP/SAVPF 111 63 103 104 9 0 8 106 105 13 110 112 113 126
  ```

  对应的数据包：

  ![image-20230518230950609](从零编写一个RTSP服务器.assets/image-20230518230950609.png)

  * media为 videos，代表这个媒体流为视频流
  * port为0：不同的使用场景，这个值赋予不同的含义：
    * rtsp使用SDP时，通常这个端口值设置为0，因为建立连接的端口是在`SETUP`方法里面指定的
    * 在国标协议里面传输rtp数据时，就需要指定rtp的端口
  * transport为`RTP/AVP`：表示传输类型
    * `RTP/AVP`：代表UDP类型（默认不加UDP）
    * TCP需要写为`RTP/AVP/TCP`
  * `fmt/payload type list` 为 96，表示它的负载类型

* rtpmap

  ```txt
  # rtpmap 可选（重要）
  a=rtpmap:<fmt/payload type><encoding name>/<clock rate>[/<encodingparameters>]
  例子：a=rtpmap:111 opus/48000/2
  ```

  对应的数据包：

  ![image-20230518232104254](从零编写一个RTSP服务器.assets/image-20230518232104254.png)

  rtpmap表示一些媒体数据，H264表示他的编码，负载类型96，90000表示时钟频率

* control

  （博客里面没找到对应的字段格式）

  对应的数据包

  ![image-20230518232419889](从零编写一个RTSP服务器.assets/image-20230518232419889.png)

  track0：表示这一路视频流的名称。用来区别不同的流，在`SETUP`建立流通道时会用到该值。

**3. `SETUP`**

![image-20230518234619165](从零编写一个RTSP服务器.assets/image-20230518234619165.png)

为视频流`track0`建立请求连接通道。

![image-20230518235018200](从零编写一个RTSP服务器.assets/image-20230518235018200.png)

* 使用的是UDP传输协议
* 端口号为`26340-26341`，每一路流都需要建立RTP和RTCP，就需要两个不同的端口

返回的数据包

![image-20230518235248989](从零编写一个RTSP服务器.assets/image-20230518235248989.png)

* 返回了客户端端口和对应的服务端端口

  ![image-20230518235334624](从零编写一个RTSP服务器.assets/image-20230518235334624.png)

* `session`对这次请求加了唯一的一个会话描述。

  不同的RTSP服务器`session`返回的时间节点可能不一样，也可以在第一次`OPTIONS`请求的返回包里面返回`session`的值，这取决于服务器的具体实现。

> 在客户端请求`SETUP`，服务端返回`SETUP`请求后，客户端和服务端会为两个端口建立两个连接，不过我们这边的服务器代码没有实现该步骤。

**4. `PLAY`**

![image-20230519000010820](从零编写一个RTSP服务器.assets/image-20230519000010820.png)

在`SETUP`之后，就开始请求`PLAY`播放视频数据。

![image-20230519000113908](从零编写一个RTSP服务器.assets/image-20230519000113908.png)

Range的值为`npt=0.000-` ： 表示一直播放，相当于直播类型的，没有结束的流媒体类型。

返回的数据包：

![image-20230519000346448](从零编写一个RTSP服务器.assets/image-20230519000346448.png)

## 可选：ffmepg入门教程

ffmpeg 是一个包含了音视频开发行业种大部分开发需求的开源项目。

主要有两种使用方式：

* 第一种方式：直接用ffmpeg提供的命令行工具
  * 优点：简单、易上手，只需熟悉一些ffmpeg的命令行文档，就可以做一些音视频的操作了。
  * 缺点：不能在同一个进程种处理音视频任务
* 第二种方式：基于ffmpeg的源代码作为依赖库开发
  * 这种方式需要至少掌握c/c++编程语言（其他语言的版本可以较不全面），在安装ffmpeg时，还需要解决ffmpeg在操作系统的编译安装等问题。

### ffmpeg 三大命令行工具

* ffmpeg 主要是多媒体的编解码工具，具体功能主要包括视频裁剪、去除水印、添加logo、提取封面、提取音频、提取视频等功能。

* ffplay 提供了音视频显示和播放相关的图像信息，音频的波形信息等。简而言之就是一个播放器。
* ffprobe 是多媒体分析工具，比如音视频的参数、媒体容器的参数信息等。也可以分析媒体文件种每个包的长度、包的类型、帧的信息等。

### 案例演示

#### 1. ffmpeg 命令行 视频生成图片

```txt
ffmpeg -i input.mp4  -r 25 -f image2  data/image%3d.jpg

image%3d.jpg 表示生成的图片序号为3个数字
image%d.jpg 表示生成的图片序号依次增加

备注：
-r 25  帧数
-f image2 格式化的格式
```

#### 2. ffmpeg 命令行 图片合成视频

将文件夹里面的frame%d.jpg格式的图片按顺序合并成视频

```cmd
ffmpeg -r 1 -f image2 -i data/%d.jpg -vcodec libx264 -s 640*480 -g 1 -keyint_min 1 -sc_threshold 0 -pix_fmt yuv420p out.mp4
```

备注：

```txt
-vcodec libx264  指定合成视频的编码格式为h264
-r 1  fps等于1 （注意这个参数需要写在 -f 之前，具体什么原因，未去研究源码。写在后面，不起效果）
-s 640*480   分辨率
-g 1  GOP长度
-keyint_min 1   keyint表示关键帧（IDR帧）间隔，这个选项表示限制IDR帧间隔最小为1帧，与设置的GOP等长
-sc_threshold 0  禁用场景识别，即进制自动添加IDR帧
-pix_fmt  yuv420p  帧格式
-vf scale=1280:-1  指定合成视频的分辨率自适应宽为1280，高按照比例计算
```

#### 3. ffmpeg 命令行 电脑的摄像头推拉流

**ffmpeg查看摄像头信息**

```txt
查看摄像头列表
ffmpeg -list_devices true -f dshow -i dummy

播放摄像头
ffplay -f dshow -i video="FULL HD webcam" # FULL HD webcam 是通过查看列表的命令行获取的名称

查看摄像头的分辨率格式
ffmpeg -list_options true -f dshow -i video="FULL HD webcam"
```

**ffmpeg摄像头推流RTSP&RTMP**

摄像头推流到RTMP服务

```cmd
ffmpeg -f dshow -i video="USB webcam" -vcodec libx264 -acodec aac -ar 44100 -ac 1 -r 25 -s 1920*1080 -f flv rtmp://192.168.1.3/live/desktop
```

摄像头推流到RTSP（rtp over tcp）

```cmd
ffmpeg -f dshow -i video="FULL HD webcam" -rtsp_transport tcp -vcodec libx264 -preset ultrafast -acodec libmp3lame -ar 44100 -ac 1 -r 25 -f rtsp rtsp://192.168.0.1/webcam
```

#### 4. ffmpeg 命令行 电脑的桌面推拉流

**ffmpeg桌面推流RTSP&RTMP**

windows桌面推流到RTMP服务

```cmd
ffmpeg -f gdigrab -i desktop -vcodec libx264 -preset ultrafast -acodec libmp3lame -ar 44100 -ac 1 -r 25 -s 1920*1080 -f flv rtmp://192.168.1.3/live/desktop
```

windows桌面推流到RTSP服务（rtp over udp）

```cmd
ffmpeg -f gdigrab -i desktop -vcodec libx264 -preset ultrafast -acodec libmp3lame -ar 44100 -ac 1 -r 25 -f rtsp rtsp://192.168.0.1/desktop
```

**ffmpeg基本推拉流命令**

RTMP推流

```cmd
ffmepg -re -i input.flv -f flv -r 25 -s 1920*1080 -an "rtmp://192.168.0.200/live/test"
```

RTSP拉流转RTMP推流

```cmd
ffmpeg -rtsp_transport tcp -i "rtsp://amdin:12345678@192.168.0.2" -f flv -c:v copy -a:v copy -r 25 -s 1920*1080 "rtmp://192.168.0.200/live/test"
```

本地视频文件RTSP推流（tcp）

```cmd
ffmpeg -re -i input.mp4 -rtsp_transport tcp -vcodec h264 -acodec copy -f rtsp rtsp://localhost/test 
```

本地视频文件RTSP推流（udp）

```cmd
ffmpeg -re -i input.mp4 -rtsp_transport udp -vcodec h264 -acodec copy -f rtsp rtsp://localhost/test
```

RTSP拉流并播放（tcp）

```cmd
ffplay -i rtsp_transport tcp rtsp://localhost/test
```

RTSP拉流并播放（udp）

```cmd
ffplay -i rtsp://localhost/test
```

#### 5. ffmpeg 基本操作命令

```txt
生成m3u8切片（1）

ffmpeg -i input.mp4 -c:v libx264 -c:a copy -f hls -hls_time 10 -hls_list_size 0
-hls_start_number 0 input/index.m3u8
备注：-hls_time n: 设置每片的长度，默认值为2。单位为秒
-hls_list_size n:设置播放列表保存的最多条目，设置为0会保存有所片信息，默认值为5
-hls_start_number n:设置播放列表中sequence number的值为number，默认值为0
-hls_wrap n:设置多少片之后开始覆盖，如果设置为0则不会覆盖，默认值为0.
   这个选项能够避免在磁盘上存储过多的片，而且能够限制写入磁盘的最多的片的数量

生成m3u8切片（2）
ffmpeg -i input.mp4 -fflags flush_packets 
-max_delay 2 -flags -global_header 
-hls_time 5 -hls_list_size 0 -vcodec libx264 -acodec aac -r 30 -g 60 index.m3u8

指定码率转换
ffmpeg -i input.mp4 -b:v 10M -b:a 10M -c:v libx264 -c:v aac out.mp4
备注：切记一点，命令行中涉及编解码时，-c:v copy 不要使用，否则 比如指定的码率参数，分辨率参数
等就会失效，而且很难找到原因
  -b:v 10M 指定视频重新编码的码率为10M/s
  -b:a 10M 指定音频重新编码的码率为10M/s


指定时间段录制
ffmpeg -i input.mp4 -c:v copy -c:a copy -ss 00:10:20 -to 00:30:20 out.mp4

指定录制时长
ffmpeg -i input.mp4 -c:v copy -c:a copy -t 30 out.mp4
备注: -t 30 表示指定30秒的录制时长

提取h264 裸码流
ffmpeg -i input.mp4 -c:v copy -bsf:v h264_mp4toannexb -an out.h264

播放h264 裸码流
ffplay -stats -f h264 out.h264
ffplay -i out.h264

提取aac 裸码流
ffmpeg -i input.mp4 -acodec copy -vn out.aac
备注：-vn (disable video)

播放aac 裸码流
ffplay -i out.aac

视频倒放，音频不变
ffmpeg -i input.mp4 -vf reverse input_video_reversed.mp4

音频倒放，视频不变
ffmpeg -i input.mp4 -map 0 -c:v copy -af  "areverse" input_audio_reversed.mp4

视频音频同时倒放
ffmpeg -i input.mp4 -vf reverse -af areverse   input_reversed.mp4


提取h264 裸码流（指定编码质量）
ffmpeg -i input.mp4 -an -c:v libx264 -crf 18 out.h264

备注： -an （disable audio） 
      -c:v libx264(等价于 -vcodec h264 或 -vcodec libx264) 
      -crf 18 (固定质量值18)

转码->AVC（指定转码的部分参数）
ffmpeg -i input.mp4 -c:v libx264 -preset slow -tune film -profile:v main out.mp4
备注：-tune film (主要配合视频类型和视觉优化的参数) 
     -preset slow 
         编码预设，主要调节 编码速度和质量的平衡
         10个选项如下 从快到慢：ultrafast、superfast、veryfast、faster、fast、medium、slow、slower、veryslow、placebo
     -profile:v main 
         h264有四种画质级别,分别是baseline, extended, main, high：
         1、Baseline Profile：基本画质。支持I/P 帧，只支持无交错（Progressive）和CAVLC；
         2、Extended profile：进阶画质。支持I/P/B/SP/SI 帧，只支持无交错（Progressive）和CAVLC；(用的少)
         3、Main profile：主流画质。提供I/P/B 帧，支持无交错（Progressive）和交错（Interlaced）， 也支持CAVLC 和CABAC 的支持；
         4、High profile：高级画质。在main Profile 的基础上增加了8x8内部预测、自定义量化、 无损视频编码和更多的YUV 格式；

转码->HEVC 
ffmpeg -i input.mp4 -c:v libx265 -c:a copy out.mp4

转码->AVC（指定转码的部分参数）
ffmpeg -i input.mp4 -c:v libx264 -b:v 2048k -vf scale=1280:-1 -y out.mp4
备注：-vf scale=1280:-1 (指定输出视频的宽高，高-1代表按照比例自动适应)
     -b:v 2048k（指定输出视频的码率，即输出视频每秒的bit数）


查看当前支持的编码器
ffmpeg -codecs
查看当前支持的封装格式
ffmpeg -formats
查看当前支持的滤镜
ffmpeg -filters

使用指定解码器播放视频
ffplay -vcodec h264 -i out.mp4

查看指定解码器的相关参数
ffmpeg -h decoder=h264_cuvid

查看当前支持的硬件加速选项
ffmpeg -hwaccels
例如：mac核显支持的选项（videotoolbox）英伟达显卡支持的选项（cuvid）

使用cuvid进行解码和编码实现转码
ffmpeg -c:v h264_cuvid -i input.mp4 -c:v h264_nvenc -b:v 2048k -vf scale=1280:-1 -y out.mp4
备注: -hwaccel cuvid (指定使用cuvid硬件加速)
     -c:v h264_cuvid (使用h264_cuvid进行视频解码)
     -c:v h264_nvenc (使用h264_nvenc进行视频编码)
     -b:v 2048k (指定输出视频的码率，即输出视频每秒的bit数)
     -vf scale=1280:-1 (指定输出视频的宽高，高-1代表按照比例自动适应)

使用videotoolbox进行编码实现转码
ffmpeg -i input.mp4 -vcodec h264_videotoolbox -b:v 2048k -vf scale=1280:-1 -y  out.mp4
备注：-vcodec h264_videotoolbox (使用h264_videotoolbox 进行视频编码)
     -b:v 2048k (指定输出视频的码率，即输出视频每秒的bit数)
     -vf scale=1280:-1 (指定输出视频的宽高，高-1代表按照比例自动适应)
```



## 第2节 实现一个传输h264的RTSP服务器

主要内容：实现一个基于UDP的RTP传输h264的RTSP服务器，并能够拉流播放。

​    上期视频，已经实现了一个RTSP协议交互的案例，客户端播放器能够向我们的RTSP服务器发起连接建立的请求，并且客户端在发起RTSP的Play请求以后，RTSP服务端也已经回复了Play请求的确认。

​    本期视频，主要实现客户端建立与RTSP服务器的连接后，并且RTSP服务端回复了客户端的Play请求以后，服务端需要源源不断的读取一个本地h264视频文件，并将读取到的h264视频流封装到RTP数据包中，再推送至客户端，这样我们就实现了一个简单的支持RTSP协议流媒体分发服务。

### RTP简介及如何将H264封装到RTP数据包

参考资料：

RTP理解：[RTP理解_屁小猪的博客-CSDN博客_rtp](https://blog.csdn.net/xt18971492243/article/details/123349467)

H264理解：[H264简介_屁小猪的博客-CSDN博客_h264](https://blog.csdn.net/xt18971492243/article/details/123360569)

H264基础知识：[H264基础知识入门_音视频开发老马的博客-CSDN博客_h264 压缩比](https://blog.csdn.net/yinshipin007/article/details/128008753?ops_request_misc={"request_id"%3A"167041712016800215015904"%2C"scm"%3A"20140713.130102334.."}&request_id=167041712016800215015904&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduend~default-2-128008753-null-null.142^v68^control,201^v4^add_ask,213^v2^t3_control2&utm_term=h264入门&spm=1018.2226.3001.4187)

#### RTP简介

RTP: 实时传输协议（Real-time Transport Protocol或简写RTP）是一个网络传输协议.
RTP定义了两种报文：RTP报文和RTCP报文，RTP报文用于传送媒体数据（如音频和视频），它由 RTP报头和数据两部分组成，RTP数据部分称为有效载荷(payload)；RTCP报文用于传送控制信息，以实现协议控制功能。RTP报文和RTCP 报文将作为下层协议的数据单元进行传输。如果使用UDP，则RTP报文和RTCP报文分别使用两个相邻的UDP端口，RTP报文使用低端口，RTCP报文使用高端口。如果使用TCP协议，RTP报文和RTCP报文可以合并，放在一个数据单元中一起传送，控制信息在前，媒体数据在后。通常，RTP是由应用程序实现的。

![image-20230608214635766](从零编写一个RTSP服务器.assets/image-20230608214635766.png)

> 也可以将RTP归为应用层的一部分

#### RTP报文格式

![image-20230608214846809](从零编写一个RTSP服务器.assets/image-20230608214846809.png)

RTP报文由两部分组成：报头和有效载荷。

RTP报头格式其中：

```txt
V：RTP协议的版本号，占2位，当前协议版本号为2。
P：填充标志，占1位，如果P=1，则在该报文的尾部填充一个或多个额外的八位组，它们不是有效载荷的一部分。
X：扩展标志，占1位，如果X=1，则在RTP报头后跟有一个扩展报头。
CC：CSRC计数器，占4位，指示CSRC 标识符的个数。
M: 标记，占1位，不同的有效载荷有不同的含义，对于视频，标记一帧的结束；对于音频，标记会话的开始。

PT: 有效载荷类型，占7位，用于说明RTP报文中有效载荷的类型，如GSM音频、JPEM图像等。
序列号：占16位，用于标识发送者所发送的RTP报文的序列号，每发送一个报文，序列号增1。接收者通过序列号来检测报文丢失情况，重新排序报文，恢复数据。
时戳(Timestamp)：占32位，时戳反映了该RTP报文的第一个八位组的采样时刻。接收者使用时戳来计算延迟和延迟抖动，并进行同步控制。

同步信源(SSRC)标识符：占32位，用于标识同步信源。该标识符是随机选择的，参加同一视频会议的两个同步信源不能有相同的SSRC。
特约信源(CSRC)标识符：每个CSRC标识符占32位，可以有0～15个。每个CSRC标识了包含在该RTP报文有效载荷中的所有特约信源。
```

RTP头结构体：

```c++
struct RtpHeader
{
    /* byte 0 */
    uint8_t csrcLen : 4;//CSRC计数器，占4位，指示CSRC 标识符的个数。
    uint8_t extension : 1;//占1位，如果X=1，则在RTP报头后跟有一个扩展报头。
    uint8_t padding : 1;//填充标志，占1位，如果P=1，则在该报文的尾部填充一个或多个额外的八位组，它们不是有效载荷的一部分。
    uint8_t version : 2;//RTP协议的版本号，占2位，当前协议版本号为2。

    /* byte 1 */
    uint8_t payloadType : 7;//有效载荷类型，占7位，用于说明RTP报文中有效载荷的类型，如GSM音频、JPEM图像等。
    uint8_t marker : 1;//标记，占1位，不同的有效载荷有不同的含义，对于视频，标记一帧的结束；对于音频，标记会话的开始。

    /* bytes 2,3 */
    uint16_t seq;//占16位，用于标识发送者所发送的RTP报文的序列号，每发送一个报文，序列号增1。接收者通过序列号来检测报文丢失情况，重新排序报文，恢复数据。

    /* bytes 4-7 */
    uint32_t timestamp;//占32位，时戳反映了该RTP报文的第一个八位组的采样时刻。接收者使用时戳来计算延迟和延迟抖动，并进行同步控制。

    /* bytes 8-11 */
    uint32_t ssrc;//占32位，用于标识同步信源。该标识符是随机选择的，参加同一视频会议的两个同步信源不能有相同的SSRC。

   /*标准的RTP Header 还可能存在 0-15个特约信源(CSRC)标识符
   
   每个CSRC标识符占32位，可以有0～15个。每个CSRC标识了包含在该RTP报文有效载荷中的所有特约信源

   */
};
```

#### H264简介

**一、H264压缩技术主要采用了以下几种方法对视频数据进行压缩**

* 帧内预测压缩，解决的是空域数据冗余问题。
* 帧间预测压缩（运动估计与补偿），解决的是时域数据冗徐问题。
* 整数离散余弦变换（DCT），将空间上的相关性变为频域上无关的数据然后进行量化。
* CABAC压缩。

经过压缩后的帧分为：I帧，P帧和B帧:

* I帧：关键帧，采用帧内压缩技术。
* P帧：向前参考帧，在压缩时，只参考前面已经处理的帧。采用帧间压缩技术。
* B帧：双向参考帧，在压缩时，它即参考前而的帧，又参考它后面的帧。采用帧间压缩技术。

除了I/P/B帧外，还有图像序列GOP。



**IDR帧和I帧的关系**

IDR(Instantannous Decoder Refresh) 解码器立即刷新

作用：在解码的过程，一旦有一帧数据出现错误，将是无法恢复的过程，后面数据帧不能使用。当有了IDR帧，解码器收到IDR帧时，就会将缓冲区的数据清空，找到第一个IDR帧，重新解码。I和IDR帧都使用帧内预测，在编码解码中为了方便，首个I帧要和其他I帧区别开，把第一个I帧叫IDR，这样方便控制编码和解码流程。IDR帧必须是一个I帧，但是I帧不一定是IDR帧，这个帧出现的时候，是告诉解码器，可以清除掉所有的参考帧，这是一个全新的序列，新的GOP已经开始。I帧有被跨帧参考的可能,IDR不会。

**二、h264组成**

1、网络提取层 (Network Abstraction Layer，NAL)
2、视讯编码层 (Video Coding Layer，VCL)

**三、码流结构**

H.264的功能分为两层，视频编码层（VCL）和网络提取层（NAL）。
1.VCL数据即被压缩编码后的视频数据序列。
2.在VCL数据要封装到NAL单元中之后，才可以用来传输或存储

![image-20230608224328068](从零编写一个RTSP服务器.assets/image-20230608224328068.png)

SPS：序列参数集，作用于一系列连续的编码图像；
PPS：图像参数集，作用于编码视频序列中一个或多个独立的图像；

NALU根据`nal_unit_type`的类型，可以分为：VCL的NAL单元和非VCL的NAL单元


#### H264码流进行RTP封装

H.264由一个一个的NALU组成，每个NALU之间使用`00 00 00 01`或`00 00 01`分隔开，每个NALU的第一次字节都有特殊的含义

![image-20230611144042162](assets/image-20230611144042162.png)

![image-20230611144059085](assets/image-20230611144059085.png)

1，F(forbiden):禁止位，占用NAL头的第一个位，当禁止位值为1时表示语法错误；

2，NRI:参考级别，占用NAL头的第二到第三个位；值越大，该NAL越重要。

3，Type:Nal单元数据类型，也就是标识该NAL单元的数据类型是哪种，占用NAL头的第四到第8个位；

**常用Nalu_type：**

```txt
0x06 (0 00 00110) SEI      type = 6
0x67 (0 11 00111) SPS      type = 7
0x68 (0 11 01000) PPS      type = 8

0x65 (0 11 00101) IDR      type = 5
0x65 (0 10 00101) IDR      type = 5
0x65 (0 01 00101) IDR      type = 5
0x65 (0 00 00101) IDR      type = 5

0x61 (0 11 00001) I帧      type = 1
0x41 (0 10 00001) P帧     type = 1
0x01 (0 00 00001) B帧     type = 1
```

对于H.264格式了解这些就够了，我们的目的是想从一个H.264的文件中将一个一个的NALU提取出来，然后封装成RTP包，下面介绍如何将NALU封装成RTP包。

####  H.264可以由三种RTP打包方式

* 单NALU打包： 一个RTP包包含一个完整的NALU
* 聚合打包：对于较小的NALU，一个RTP包可包含多个完整的NALU
* 分片打包：对于较大的NALU，一个NALU可以分为多个RTP包发送

注意：这里要区分好概念，每一个RTP包都包含一个RTP头部和RTP荷载，这是固定的。而H.264发送数据可支持三种RTP打包方式。

比较常用的是单NALU打包和分片打包，这里只介绍两种：

**单NALU打包**

所谓单NALU打包就是将一整个NALU的数据放入RTP包的载荷中，这是最简单的一种方式。

**分片打包**

每个RTP包都有大小限制的，因为RTP一般都是使用UDP发送，UDP没有流量控制，所以要限制每一次发送的大小，所以如果一个NALU的太大，就需要分成多个RTP包发送，至于如何分成多个RTP包，如下：

首先要明确，RTP包的格式是绝不会变的，永远多是RTP头+RTP载荷

![image-20230611144627357](assets/image-20230611144627357.png)

RTP头部是固定的，那么只能在RTP载荷中去添加额外信息来说明这个RTP包是表示同一个NALU

如果是分片打包的话，那么在RTP载荷开始有两个字节的信息，然后再是NALU的内容

![image-20230611144654210](assets/image-20230611144654210.png)

<font color=red>第一个字节位`FU Indicator`，其格式如下</font>

![image-20230611144755397](assets/image-20230611144755397.png)

* `高三位`：与NALU第一个字节的高三位相同

* `Type`：28，表示该RTP包一个分片，为什么是28？因为H.264的规范中定义的，此外还有许多其他Type，这里不详讲。

<font color=red>第二个字节位`FU Header`，其格式如下</font>

![image-20230611144909196](assets/image-20230611144909196.png)

* `S`：标记该分片打包的第一个RTP包

* `E`：比较该分片打包的最后一个RTP包

* `R` (1 bit): 这是保留位，按照 RFC 规定，这个位必须被设置为 0。

* `Type`：NALU 的 Type


### 实验：实现一个h264的RTSP服务器

ffmpeg命令行解封装.mp4生成.h264视频文件

```cmd
ffmpeg -i test.mp4 -codec copy -bsf: h264_mp4toannexb -f h264 test.h264
```

> 该命令行运行快速的原因：
>
> mp4文件产生h264文件本身只是一个解协议，去掉mp4的头尾，提取出码流，然后直接封装成文件。不涉及任何编解码，如果该mp4里面不是h264，而是其他格式（如h265），则需要设置编码器，运行速度就比较慢。

命令参数说明：

* `-i`：指定输入文件
* `-code`：指定编码器，因为该mp4本身就是h264编码，所以设置为`copy`
* `-bsf`：指定解封装的格式。因为在把h264封装成mp4时，有两种封装类型，所以解封装时同样要指定封装类型。
* `-f`：产生的结果类型

实验：

* 启动study2的代码
* 命令行启动:`ffplay -i rtsp://127.0.0.1:8554`

## 可选：音频原理

参考文章：

* 一文看懂音频文章：`https://www.bilibili.com/read/cv1771961`
* PCM数据格式：`https://blog.csdn.net/whybiang/article/details/6632564`
* 

### 音频的基本概念

* 频率（音调）：声音1秒内周期性变化的次数

* 振幅（响度）：声音的大小

  用分贝（dB）来形容声音的大小。dB是一个比值，是一个数值，没有任何单位标注。（功率强度之比的对数的10倍）

**声音的采样与存储**

**采样**，指把时间域或空间域的连续量转化成离散量的过程 。

对声音的采样常用麦克风等设备将声音信号转换成电信号，再用模/数转换器将电信号转换成一串用1和0表示的二进制数字（数字信号）。

我们每秒对声音采样上万次，获得上万个按照时间顺序排列的二进制数字。于是，我们就将连续变化不断的声音转化成了计算机可储存并识别的二进制数字。

**采样频率：**指录音设备在一秒钟内对声音信号的采样次数。采样频率越高，声音的还原就越真实越自然。

> 目前主流的采样频率有22.05KHz、44.1KHz、48KHz三种。
>
> 22.05 KHz为FM广播的声音品质，44.1KHz为理论上的CD声音品质。48KHz为人耳可辨别的最高采样频率。 

**PCM数据格式**

PCM(Pulse Code Modulation)也被称为 脉码编码调制。PCM中的声音数据没有被压缩，如果是单声道的文件，采样数据按时间的先后顺序依次存入。(它的基本组织单位是BYTE(8bit)或WORD(16bit))。

一般情况下，一帧PCM是由2048次采样组成的。

如果是双声道的文件，采样数据按时间先后顺序交叉地存入。如图所示:

![image-20230709161834715](assets/image-20230709161834715.png)

录音得到的PCM原始声音，码流通常比较大，在实际音频传输或保存过程中，都是需要对其进行编码压缩后传输或保存的。

```txt
1分钟的PCM的声音大小 = 44100*2（声道）*2(字节) /1024 /1024 *60 约等于 5MB
```

**例程：音频的操作demo**

项目地址：`https://gitee.com/Vanishi/BXC_AudioDemo`





## 第3节：实现一个传输音频aac的RTSP服务器

上期视频，我们已经实现了一个基于UDP的RTP传输h264的RTSP服务器，客户端播放器能够向我们的RTSP服务端发起连接建立的请求，并且客户端在发起RTSP的Play请求以后，RTSP服务端在回复了Play请求之后，开始源源不断的通过RTP协议向客户端推送h264视频流。

本期视频我们需要实现，客户端建立与RTSP服务端的连接后，并且在RTSP服务端回复了客户端的Play请求以后，服务端需要源源不断的读取一个本地aac音频文件，并将读取到的aac音频码流封装到RTP数据包中，再推送至客户端。这样我们就实现了一个简单的支持RTSP协议的流媒体分发音频流的服务。

> 音频原理 课程的概要
>
> 1. 麦克风在录音采样后，直接获得的就是音频原始数据pcm。但由于pcm数据较大，所以通常需要压缩之后才能传输或保存，常见的音频压缩技术有aac，g711，opus，mp3等，最常用的就是aac。
> 2. 介绍了一个音频的项目，开源地址：https://gitee.com/Vanishi/BXC_AudioDemo，这个项目基于fdk-aac和sdl2两个依赖库，分别实现了解码音频aac然后播放，直接解析pcm然后播放。

### AAC 音频格式

AAC音频格式有2种：

* ADIF（Audio Data Interchange Format）：音频数据交换格式，只有一个统一的头，必须得到所有数据后解码，适用于本地文件。

* ADTS（Audio Data Transport Stream）：音频数据传输流，每一帧都有头信息，任意帧解码，适用于传输流。

  > 注意：这里所说的音频帧不是音频的采样点，采集的采样率为1s 44100次，通常情况下是1024个采样点作为一个音频帧，然后进行编码和存储

我们主要介绍ADTS。

#### AAC的头介绍

参考文章 ：C++ 解析aac-adts的头部信息 `https://blog.csdn.net/u013113678/article/details/123134860`

adts的头部一共有15个字段，共7bytes，如果有校验位则会在尾部增加2bytes的CRC校验，具体如下：

```c++
struct AdtsHeader {
    unsigned int syncword;  //12 bit 同步字 '1111 1111 1111'，一个ADTS帧的开始
    uint8_t id;        //1 bit 0代表MPEG-4, 1代表MPEG-2。
    uint8_t layer;     //2 bit 必须为0
    uint8_t protectionAbsent;  //1 bit 1代表没有CRC，0代表有CRC
    uint8_t profile;           //2 bit AAC级别（MPEG-2 AAC中定义了3种profile，MPEG-4 AAC中定义了6种profile）
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
```

#### ffmpeg 命令行提取音频文件

```cmd
//ffmpeg命令行 从mp4视频文件提取aac 音频文件
ffmpeg -i test.mp4  -vn -acodec aac test.aac
备注：-i 表示输入文件 
      -vn disable video / 丢掉视频
      -acodec 设置音频编码格式

//ffmpeg 从aac音频文件解码为pcm音频文件
ffmpeg -i test.aac -f s16le test.pcm
备注：-i 表示输入文件 
      -f 表示输出格式

//ffplay 播放.pcm音频文件
ffplay -ar 44100 -ac 2 -f s16le -i test.pcm
备注：-i 表示指定的输入文件
      -f 表示强制使用的格式
      -ar 表示播放的音频数据的采样率
      -ac 表示播放的音频数据的通道数
```

> 默认情况下，FFmpeg生成的AAC音频文件是ADTS格式。这是因为ADTS格式是一种常见的AAC音频封装格式，它包含有音频帧同步信息和一些其他的元数据，比ADIF格式更为完整。

#### AAC的RTP打包

参考：RTSP协议的一些分析（六）——使用RTP传输AAC文件  https://blog.csdn.net/yangguoyu8023/article/details/106517251/

AAC的RTP打包方式并没有向H.264那样丰富，我知道的只有一种方式，原因主要是AAC一帧数据大小都是几百个字节，不会向H.264那么少则几个字节，多则几千。
AAC的RTP打包方式就是将ADTS帧取出ADTS头部，取出AAC数据，每帧数据封装成一个RTP包
需要注意的是，并不是将AAC数据直接拷贝到RTP的载荷中。AAC封装成RTP包，在RTP载荷中的前四个字节是有特殊含义的，然后再是AAC数据，如下图所示
![image-20230613233314243](从零编写一个RTSP服务器.assets/image-20230613233314243.png)

其中RTP载荷的一个字节为0x00，第二个字节为0x10。第三个字节和第四个字节保存AAC Data的大小，最多只能保存13bit，第三个字节保存数据大小的高八位，第四个字节的高5位保存数据大小的低5位。

```c++
rtpPacket->payload[0] = 0x00;
rtpPacket->payload[1] = 0x10;
rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5; //高8位
rtpPacket->payload[3] = (frameSize & 0x1F) << 3; //低5位
```

实验：实现一个aac的RTSP服务器

代码：`study3`

## 第4讲 实现一个基于TCP的RTP同时传输h264的aac的RTSP服务器

本节内容：实现一个基于TCP的RTP同时传输h264和aac的RTSP服务器，并能够拉流播放

> 本系列的前三期视频，我们已经了解和实现了RTSP协议，以及实现了基于UDP的RTP分别传输h264视频流和aac音频流。

在前三期视频的学习基础上，本期其实比较简单，相对于之前实现的功能，变化如下:

1. 客户端请求RTSP的Describe请求时，RTSP服务器返回的SDP协议，需要同时包含音频流和视频流的信息。

2. 客户端请求RTSP的Setup请求时，RTSP服务器不需要再对应创建RTP和RTCP的UDP连接通道，因为TCP版的RTP传输，客户端与服务器交互时，无论是RTSP信令还是RTP数据包或者是RTCP数据包，都是使用同一个tcp连接通道。只不过这个tcp连接通道在发送rtp数据包或者rtcp数据包时，需要加一些分隔字节。

3. 客户端请求RTSP的Play请求时，RTSP服务器在对Play请求回复以后，还需要源源不断的同时向客户端发送音频流和视频流的RTP数据包。

4. 有几点注意，在这个案例项目中，使用的h264视频文件，对应的fps需要是25。另外由于Nalu的数量并不等于视频帧数量的原因，该案例的音视频并不能同步。

```txt
//ffmpeg命令行 从mp4视频文件提取h264 码流文件
ffmpeg -i test.mp4 -an -vcodec copy -f h264 test.h264

//ffmpeg命令行 从mp4视频文件提取aac 音频文件
ffmpeg -i test.mp4  -vn -acodec aac test.aac
备注：-i 表示输入文件 
      -vn disable video / 丢掉视频
```



## 可选：音视频并发测试工具，C++开发的流媒体服务器并发测试工具，可以用来测试流媒体服务器的并发性能

### 1. 流媒体并发测试工具 `BXC_mstest`

> 开源地址：`https://gitee.com/Vanishi/BXC_mstest`

#### 1.1 介绍

1. 基于C++开发的流媒体并发测试工具，可以测试流媒体并发性能，也可以测试电脑或服务器等设备能够支持的最大解码并发数
2. 支持Windows/Linux
3. 该工具的核心就是借助ffmpeg进行拉流，一旦拉流失败就并发数下降

#### 1.2 编译说明

1. 推荐使用visual studio 2019
2. 使用visual studio 2019 打开 BXC_mstest.sln，设置成x64/Release模式，依赖库已经配置，可以直接运行，如果提示缺少dll，请从3rdparty找到dll拷贝到程序执行目录

#### 1.3 使用说明

```txt
首先，打开一个cmd窗口，在cmd窗口中输入 mstest.exe -h 查看参数

-h 查看帮助文档
-t 测试类型 （0:拉流 1:拉流+解码）如：-t 0
-c 测试并发数   如：-c 2
-i 视频流地址（也可以是本地视频文件路径）  如：-i rtsp://127.0.0.1:554/live/test
-v 视频流解码器  如：h264,h264_qsv,h264_cuvid,hevc,hevc_qsv,hevc_cuvid
-r rtsp拉流时传输层协议(udp或tcp)  如：-r udp
-s 记录日志的间隔时间，休眠单位为毫秒  如：-s 1000
```



### 2. 准备流媒体服务器

测试`BXC_mstest`需要流媒体服务器，推荐使用`ZLMediaKit`流媒体服务器

* 开源地址：
  * `https://gitee.com/xia-chu/ZLMediaKit`
  * `https://github.com/ZLMediaKit/ZLMediaKit`
* 北小菜编译好的windows版本：`https://gitee.com/Vanishi/zlm`

#### 2.1 使用方法

<font color=blue>1. 启动流媒体服务器</font>

> 这边使用北小菜编译好的版本

`V2024.9.14.windows`进到该命令，执行`start.bat`

![image-20250426131547305](从零编写一个RTSP服务器.assets/image-20250426131547305.png)

<font color=blue>2. 往流媒体服务器推流：</font>

**流媒体服务器是没有任何流的。**

**开启流媒体服务器以后，推荐使用如下几种方式为流媒体服务器推流：**

* [FFmpeg推USB摄像头](https://gitee.com/link?target=https%3A%2F%2Fbeixiaocai.yuque.com%2Forg-wiki-beixiaocai-vo72oa%2Fxcms%2F0deb4194d8ad8f0ca437263dd40597c0)

  * 命令：

    ```txt
    （1）查看所有插在电脑上的USB摄像头列表
    ffmpeg -list_devices true -f dshow -i dummy
    
    （2）播放摄像头
    ffplay -f dshow -i video="FULL HD webcam"
    
    注意："FULL HD webcam" 是通过查看USB摄像头列表的命令行获得的设备名称
    
    （3）查看摄像头的分辨率格式
    ffmpeg -list_options true -f dshow -i video="FULL HD webcam"
    
    （4）将USB摄像头推流到视频行为分析系统
    ffmpeg -f dshow -i video="FULL HD webcam" -rtsp_transport tcp -c:v h264 -pix_fmt yuv420p -r 25 -s 1920*1080 -f rtsp rtsp://127.0.0.1:9554/live/camera
    
    注意：
    （1）rtsp://127.0.0.1:9554/live/camera 并不是固定的推流地址，可以按照自己的需求修改
    ```

* [FFmpeg推本地视频文件](https://gitee.com/link?target=https%3A%2F%2Fbeixiaocai.yuque.com%2Forg-wiki-beixiaocai-vo72oa%2Fxcms%2F60850e5f979a8cddc42d5fd4e81fc70f)

  * 测试视频：

    * 百度网盘：https://pan.baidu.com/s/1uNx8eSttA1qCVlgfii0VtA?pwd=xcxc 
    * 夸克网盘：https://pan.baidu.com/s/1uNx8eSttA1qCVlgfii0VtA?pwd=xcxc 

  * 命令：

    ```cmd
    （1）将test.mp4循环推流至xcms(rtsp推流)
    ffmpeg -re -stream_loop -1 -i test.mp4  -rtsp_transport tcp -c copy -f rtsp rtsp://127.0.0.1:9554/live/test
    
    （2）将test.mp4循环推流至xcms(转码+rtsp推流)
    ffmpeg -re -stream_loop -1 -i test.mp4  -rtsp_transport tcp -c:v libx264 -an -f rtsp rtsp://127.0.0.1:9554/live/test
    
    （3）将test.mp4循环推流至xcms(rtmp推流)
    ffmpeg -re -stream_loop -1  -i test.mp4 -c:v copy -an -f flv rtmp://192.168.1.11:9935/live/test
    
    （4）将test.mp4循环推流至xcms(转码+rtmp推流)
    ffmpeg -re -stream_loop -1  -i test.mp4 -c:v libx264 -an -f flv rtmp://192.168.1.11:9935/live/test
    
    （5）将 "其他RTSP视频流地址" 转推至xcms(rtsp转推流)，转推前确保 "其他RTSP视频流地址" 可以正常播放，可以用vlc播放测试
    ffmpeg -i "其他RTSP视频流地址"  -rtsp_transport tcp -c copy -f rtsp rtsp://127.0.0.1:9554/live/test
    
    （6）将 "其他RTSP视频流地址" 转推至xcms(转码+rtsp转推流)，转推前确保 "其他RTSP视频流地址" 可以正常播放，可以用vlc播放测试
    ffmpeg -i "其他RTSP视频流地址"  -rtsp_transport tcp -c:v libx264 -an -f rtsp rtsp://127.0.0.1:9554/live/test
    
    注意：
    （1）rtsp://127.0.0.1:9554/live/test并不是固定的推流地址，可以按照自己的需求修改
    （2）rtmp转推rtsp流或rtsp转推rtmp流都是可以的，可以参考上面的命令行学习和扩展
    ```

* [FFmpeg电脑桌面推流](https://gitee.com/link?target=https%3A%2F%2Fbeixiaocai.yuque.com%2Forg-wiki-beixiaocai-vo72oa%2Fxcms%2F00a95632b189aa5a4e5e6e4ffb2bbd40)

  * 命令：

    ```cmd
    （1）实时采集Windows桌面并推流至xcms（rtsp推流）
    ffmpeg -f gdigrab -i desktop -vcodec libx264 -preset ultrafast -acodec libmp3lame -ar 44100 -ac 1 -r 25 -f rtsp rtsp://127.0.0.1:9554/live/desktop
    
    注意：
    （1）rtsp://127.0.0.1:9554/live/desktop并不是固定的推流地址，可以按照自己的需求修改
    ```

补充说明： 

* ffmpeg命令行的下载链接：https://pan.quark.cn/s/bc3a69677a8f 提取码：SBCp

* 注意事项：
  * 通过ffmpeg命令行进行视频推流，视频编码推流，视频转推流等延迟是无法避免的

  * 解决ffmpeg延迟问题，可以参考视频：

    [C++/FFmpeg开发零延迟的摄像头拉流转码推流器_哔哩哔哩_ bilibili](https://www.bilibili.com/video/BV1RN411K75R/?spm_id_from=333.788&vd_source=f70a17c45cd3d279ce923ca59e12fff0)

**这边使用FFmpeg 推本地视频文件**

```cmd
ffmpeg -re -stream_loop -1 -i test.mp4  -rtsp_transport tcp -c copy -f rtsp rtsp://127.0.0.1:554/live/test
```

流媒体服务器的输出日志：

![image-20250426131706924](从零编写一个RTSP服务器.assets/image-20250426131706924.png)

可以看到，执行完推流命令后，有一路`live/test`流，这路流过来以后就被分发成 rtsp/fmp4/rtmp/hls等协议的相关流

### 3. 启动测试工具`BXC_mstest`

编译出来到debug目录并执行

```cmd
.\mstest.exe -c 5 -i rtsp://127.0.0.1:554/live/test -r udp
```

> 执行过程中可能缺少库，在`3rdparty`中进行拷贝





## 其他

### 下载ffmpeg

到`https://github.com/BtbN/FFmpeg-Builds/releases`下载一个编译好的命令行工具，并把目录添加到环境变量中。

![image-20230517231938014](从零编写一个RTSP服务器.assets/image-20230517231938014.png)



## 函数补充

### `WSAStartup()`

```c++
int WSAStartup(
        WORD      wVersionRequired,
  [out] LPWSADATA lpWSAData
);
```

`WSAStartup`函数用于初始化Winsock库，通常在使用网络功能的程序的开头进行调用。它接受两个参数：

1. `wVersionRequested`：该参数指定应用程序打算使用的Winsock版本。如传入`MAKEWORD(2, 2)`，它请求使用常用的2.2版本的Winsock。
2. `lpWSAData`：这个参数是指向`WSADATA`结构的指针，该结构用于接收有关Winsock实现的信息。该结构包含各种详细信息，例如实际使用的Winsock版本、实现的详细信息以及任何可用的错误信息。

`WSAStartup`函数如果成功初始化Winsock库则返回零；否则，返回一个指示错误的值。

### `strtok()`

`strtok()` 函数是 C 语言中的字符串处理函数，用于将字符串按照指定的分隔符进行分割。

函数原型如下：

```c++
char *strtok(char *str, const char *delim);
```

`strtok()` 函数接受两个参数：

- `str`：需要分割的字符串。在第一次调用时，传入要分割的字符串，在后续调用中，传入 `NULL`，函数会继续处理上一次的字符串。
- `delim`：分割符字符串。用于指定分割字符串的分隔符，可以是一个或多个字符。函数会将字符串按照这些字符进行分割。

`strtok()` 函数的返回值是一个指向被分割出的子字符串的指针。在第一次调用时，返回的指针指向第一个分割出的子字符串，后续调用可以通过传入 `NULL` 获取下一个分割出的子字符串。

`strtok()` 函数在使用时需要注意以下几点：

1. 原始字符串会被修改，分割符会被替换为字符串结束符（`\0`）。
2. 在多线程环境下，`strtok()` 函数不是线程安全的，可以使用 `strtok_r()` 函数来替代。
3. 如果原始字符串中包含连续的分割符，`strtok()` 函数会忽略它们，不会返回空字符串。

以下是一个简单的示例代码，演示了如何使用 `strtok()` 函数进行字符串分割：

```c++
#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "Hello,World,How,Are,You";
    const char delim[] = ",";

    char *token = strtok(str, delim);
    while (token != NULL) {
        printf("%s\n", token);
        token = strtok(NULL, delim);
    }

    return 0;
}
```

以上代码会将字符串 `"Hello,World,How,Are,You"` 按照逗号分隔符进行分割，并逐个输出分割后的子字符串。

```bash
Hello
World
How
Are
You
```

### `strstr()`

`strstr()` 函数是 C 语言中的字符串处理函数，用于在一个字符串中查找指定子字符串的第一次出现位置。

函数原型如下：

```c++
char *strstr(const char *haystack, const char *needle);
```

`strstr()` 函数接受两个参数：

- `haystack`：要在其中查找子字符串的源字符串。
- `needle`：要查找的子字符串。

`strstr()` 函数会在 `haystack` 字符串中查找第一次出现的 `needle` 子字符串，并返回一个指向该位置的指针。如果找到了子字符串，则返回的指针指向 `haystack` 字符串中该子字符串的首字符；如果未找到子字符串，则返回 `NULL`。

### `sscanf()`

`sscanf()` 函数是 C 语言中的输入格式化函数，用于从一个字符串中按照指定的格式提取数据。

函数原型如下：

```c++
int sscanf(const char *str, const char *format, ...);
```

`sscanf()` 函数接受两个必需参数：

- `str`：包含要解析的输入数据的字符串。
- `format`：指定输入数据的格式的字符串。格式字符串包含特定的转换说明符，用于指定要提取的数据类型和其它格式信息。

`sscanf()` 函数可以接受多个可选参数，用于存储提取到的数据。这些参数应该是指向已经分配内存的变量的指针，以便将提取的数据存储在这些变量中。

`sscanf()` 函数会根据格式字符串的规则从输入字符串中解析数据，并根据指定的格式将其存储在对应的变量中。解析过程会根据格式字符串中的转换说明符进行匹配和转换。

以下是一个简单的示例代码，演示了如何使用 `sscanf()` 函数从字符串中提取数据：

```c++
#include <stdio.h>

int main() {
    const char str[] = "John Doe 25";
    char name[20];
    int age;

    sscanf(str, "%s %*s %d", name, &age);
    printf("Name: %s\n", name);
    printf("Age: %d\n", age);

    return 0;
}
```

以上代码会从字符串 `"John Doe 25"` 中提取姓名和年龄。`%s` 转换说明符用于提取字符串，`%d` 转换说明符用于提取整数。`%*s` 转换说明符用于跳过一个字符串，这里用于跳过中间的 "Doe"。

输出结果：

```bash
Name: John
Age: 25
```







