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

* RTSP协议：负责服务器与客户端之间的请求与相应
* RTP协议：负责服务器与客户端之间传输媒体数据
* RTCP协议：负责提供有关RTP传输质量的反馈，就是确保RTP传输的质量

三者的关系：

* rstp并不会发送媒体数据，只是完成服务器和客户端之间的信令交互，rtp协议负责媒体数据传输，rtcp负责rtp数据包的监视和反馈。
* rtp和rtcp并没有规定传输层的类型，可以选中udp和tcp。
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

> 在客户端请求`SETUP`，服务端返回`SETUP`请求后，客户端和服务端会为两个端口建立两个连接，不同我们这边的服务器代码没有实现该步骤。

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
  * 优点：简单、易上手，只需属性一些ffmpeg的命令行文档，就可以做一些音视频的操作了。
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

