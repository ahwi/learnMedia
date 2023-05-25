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

* rtp并不会发送媒体数据，只是完成服务器和客户端之间的信令交互，rtp协议负责媒体数据传输，rtcp负责rtp数据包的监视和反馈。
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

