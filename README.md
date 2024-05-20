## 1.总览
rocket-rpc 是基于 C++11 开发的一款多线程的异步 RPC 框架，它旨在高效、简洁的同时，又保持至极高的性能。

rocket-rpc 同样是基于主从 Reactor 架构，底层采用 epoll 实现 IO 多路复用。应用层则基于 protobuf 自定义 rpc 通信协议，同时也将支持简单的 HTTP 协议。

### 1.1 技术栈
- c++11
- protobuf
- rpc
- reactor
- http

### 1.2 前置知识
- 掌握 C++ 语言，至少能熟悉主要的语法、面向对象编程方法等
- 掌握 Linux 环境，熟悉 Linux 网络编程、Socket 编程 等
- 熟悉 Reactor 架构
- 了解 Git 及相关命令
- 了解计算机网络相关知识，如 Tcp 等
- 了解 Protobuf，可以能看懂和编写简单的 protobuf 协议文件
- 了解 rpc 通信原理

### 1.3 项目计划
整个项目按以下大纲进行：
- 前置准备，包括环境搭建、依赖库安装
- 日志及配置类开发
- Reactor 核心模块
- Tcp 模块封装
- 序列化、编解码模块
- Rpc 模块封装
- 脚手架搭建
- 简单性能测试


## 2. 前置准备
### 2.1 环境搭建
- 开发环境：Linux，可以是虚拟机。如Centos8 的虚拟机. 包含必要的 C++ 开发工具，如 GCC/G++(至少能支持到 C++11 语法的版本)
- 开发工具：VsCode，通过 ssh 远程连接 linux 机器

### 2.2 依赖库的安装
#### 2.2.1 protobuf
protobuf 推荐使用 3.19.4 及其以上：

安装过程：
```
wget  https://github.com/protocolbuffers/protobuf/releases/download/v3.19.4/protobuf-cpp-3.19.4.tar.gz
tar -xzvf protobuf-cpp-3.19.4.tar.gz
```

我们需要指定 安装路径在 `/usr` 目录下:
```
cd protobuf-cpp-3.19.4
./configure -prefix=/usr/local
make -j4 
sudo make install
```

安装完成后，你可以找到头文件将位于 `/usr/include/google` 下，库文件将位于 `/usr/lib` 下。
也可能在`/usr/local/include/google`下，库文件则对应于`/usr/local/lib`下。

#### 2.2.2 tinyxml
项目中使用到了配置模块，采用了 xml 作为配置文件。因此需要安装 libtinyxml 解析 xml 文件。

```
wget https://udomain.dl.sourceforge.net/project/tinyxml/tinyxml/2.6.2/tinyxml_2_6_2.zip
unzip tinyxml_2_6_2.zip
修改Makefile文件的84行, 将输出变为libtinyxml.a
make -j4
sudo cp libtinyxml.a /usr/lib/
sudo mkdir /usr/include/tinyxml
sudo cp tinyxml/*.h /usr/include/tinyxml
```

### 2.3 日志模块开发
首先需要创建项目：

日志模块：
```
1. 日志级别
2. 打印到文件，支持日期命名，以及日志的滚动。
3. c 格式化风控
4. 线程安全
```

LogLevel:
```
Debug
Info
Error
```

LogEvent:
```
文件名、行号
MsgNo
进程号
Thread id
日期，以及时间。精确到 ms
自定义消息
```

日志格式
```
[Level][%y-%m-%d %H:%M:%s.%ms]\t[pid:thread_id]\t[file_name:line][%msg]
```

Logger 日志器
1.提供打印日志的方法
2.设置日志输出的路径

### 2.4 Reactor
Reactor，又可以称为 EventLoop，它的本质是一个事件循环模型。服务器有一个 MainReactor 和多个 SubReactor。

MainReactor 由主线程运行，作用如下：通过 epoll 监听 listenfd 的可读事件，当可读事件发生后，调用 accept 函数获取 clientfd，然后随机取出一个 SubReactor，将 cliednfd 的读写事件注册到这个 SubReactor 的 epoll 上即可。也就是说，MainReactor 只负责建立连接事件，不进行业务处理，也不关心已连接套接字的 IO 事件。

SubReactor 通常有多个，每个 SubReactor 由一个线程来运行。SubReactor的 epoll 中注册了 clientfd 的读写事件，当发生 IO 事件后，需要进行业务处理。

#### 2.4.1 TimerEvent 定时任务
```
1. 指定时间点 arrive_time
2. interval, ms。
3. is_repeated 
4. is_cancled
5. task


cancel()
cancelRepeated()
```

#### 2.4.2 Timer
定时器，它是一个 TimerEvent 的集合。
Timer 继承 FdEvent
```

addTimerEvent();
deleteTimerEvent();

onTimer();    // 当发生了 IO 事件之后，需要执行的方法


resetArriveTime()

multimap 存储 TimerEvent <key(arrivetime), TimerEvent>
```

#### 2.5 IO 线程
创建一个IO 线程，他会帮我们执行：
1. 创建一个新线程（pthread_create）
2. 在新线程里面 创建一个 EventLoop，完成初始化
3. 开启 loop
```
class {
  
 pthread_t m_thread;
 pid_t m_thread_id;
 EventLoop event_loop;
}

```

RPC 服务端流程
```
启动的时候就注册 OrderService 对象.

1. 从 buffer 读取数据, 然后 decode 得到请求的 TinyPBProtocol 对象. 
2. 然后从请求的 TinyPBProtocol 得到 method_name, 从 OrderService 对象里根据service.method_name 找到方法 func
3. 找到对应的 request type 以及 response type
4. 将请求体 TinyPBProtocol 里面的 pb_data 反序列化为 request type 的一个对象, 并声明一个空的 response type 对象
5. 调用 func(request, response)
6. 将 response 对象序列化为 pb_data, 再塞入到 TinyPBProtocol 结构体中. 之后 encode 并塞入到 buffer 里面, 监听可写事件发送回包
```