# mini-muduo 学习型任务规划

## 目标

通过手写一个简化版 muduo 网络库，边写边学习：

- Linux C++ 编译与调试
- TCP socket 编程
- 非阻塞 IO
- epoll
- Reactor 模式
- Buffer 与 TCP 粘包/半包处理
- 连接管理
- 多线程与线程池
- 定时器
- 简易 RPC 框架

最终目标是实现一个简化网络库：

```text
EventLoop
Channel
EpollPoller
Buffer
TcpConnection
Acceptor
TcpServer
EventLoopThread
EventLoopThreadPool
TimerQueue
```

最终可以写出类似代码：

```cpp
int main() {
    EventLoop loop;
    InetAddress addr(9000);

    TcpServer server(&loop, addr, "EchoServer");

    server.setConnectionCallback(onConnection);
    server.setMessageCallback(onMessage);

    server.start();
    loop.loop();
}
```

---

## 阶段 0：环境准备

### 学习点

```text
g++
cmake
gdb
Linux 基础命令
```

### 任务

创建项目结构：

```text
mini-muduo/
  CMakeLists.txt
  examples/
    echo_server.cc
  muduo/
    base/
    net/
```

### 验收

能编译运行：

```cpp
int main() {
    std::cout << "hello mini-muduo\n";
}
```

命令：

```bash
cmake -B build
cmake --build build
./build/examples/echo_server
```

---

## 阶段 1：手写阻塞 TCP Echo Server

### 学习点

```text
socket
bind
listen
accept
recv
send
close
```

### 任务

写一个最朴素版本：

```text
单线程
阻塞 accept
阻塞 recv
一次只处理一个 client
收到什么回什么
```

### 文件

```text
examples/blocking_echo_server.cc
```

### 验收

终端 1：

```bash
./blocking_echo_server
```

终端 2：

```bash
nc 127.0.0.1 9000
hello
```

server 回：

```text
hello
```

---

## 阶段 2：支持多个 client：epoll 原始版

### 学习点

```text
非阻塞 socket
epoll_create1
epoll_ctl
epoll_wait
EPOLLIN
```

### 任务

写一个不封装的 epoll echo server：

```text
listen_fd 注册 epoll
client_fd 注册 epoll
哪个 client 可读就 recv
收到什么 send 回去
```

### 文件

```text
examples/epoll_echo_server.cc
```

### 验收

开多个终端：

```bash
nc 127.0.0.1 9000
```

多个 client 同时收发不互相阻塞。

---

## 阶段 3：封装 Channel

### 学习点

```text
Reactor 基本单元
fd 和事件回调绑定
std::function
```

### Channel 负责

```text
保存 fd
保存关心事件 events
保存实际发生事件 revents
保存 read/write/close/error callback
handleEvent() 调用回调
```

### 文件

```text
muduo/net/Channel.h
muduo/net/Channel.cc
```

### 最小接口

```cpp
class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(int fd);

    int fd() const;
    uint32_t events() const;
    void set_revents(uint32_t revt);

    void enableReading();
    void disableAll();

    void setReadCallback(EventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

    void handleEvent();

private:
    int fd_;
    uint32_t events_;
    uint32_t revents_;

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
```

### 验收

原始 epoll server 改成：

```text
epoll 返回 fd
找到 Channel
channel->set_revents(events)
channel->handleEvent()
```

---

## 阶段 4：封装 EpollPoller

### 学习点

```text
epoll 封装
fd -> Channel* 映射
RAII
```

### Poller 负责

```text
管理 epoll fd
注册/修改/删除 Channel
epoll_wait 返回活跃 Channel 列表
```

### 文件

```text
muduo/net/EpollPoller.h
muduo/net/EpollPoller.cc
```

### 最小接口

```cpp
class EpollPoller {
public:
    using ChannelList = std::vector<Channel*>;

    EpollPoller();
    ~EpollPoller();

    void poll(int timeoutMs, ChannelList* activeChannels);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    int epollfd_;
    std::vector<epoll_event> events_;
    std::unordered_map<int, Channel*> channels_;
};
```

### 验收

epoll 细节从 example 中消失。example 只关心：

```cpp
poller.poll(10000, &activeChannels);
for (auto ch : activeChannels) {
    ch->handleEvent();
}
```

---

## 阶段 5：封装 EventLoop

### 学习点

```text
Reactor 事件循环
one loop per thread 雏形
```

### EventLoop 负责

```text
拥有 EpollPoller
循环 poll
分发 activeChannels
提供 updateChannel/removeChannel
```

### 文件

```text
muduo/net/EventLoop.h
muduo/net/EventLoop.cc
```

### 最小接口

```cpp
class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    bool looping_;
    bool quit_;
    EpollPoller poller_;
    std::vector<Channel*> activeChannels_;
};
```

### 验收

example 变成：

```cpp
EventLoop loop;
Channel listenChannel(listen_fd);
listenChannel.setReadCallback(...);
listenChannel.enableReading();
loop.loop();
```

---

## 阶段 6：封装 Acceptor

### 学习点

```text
监听 socket 生命周期
accept 新连接回调
```

### Acceptor 负责

```text
创建 listen_fd
bind/listen
accept 新连接
把 client_fd 通过 callback 抛给 TcpServer
```

### 文件

```text
muduo/net/Acceptor.h
muduo/net/Acceptor.cc
muduo/net/InetAddress.h
muduo/net/Socket.h
```

### 最小接口

```cpp
class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb);
    void listen();

private:
    void handleRead();

    EventLoop* loop_;
    int listenfd_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
};
```

### 验收

主程序不用再写 `socket/bind/listen/accept`。

---

## 阶段 7：封装 Buffer + 解决粘包半包

### 学习点

```text
TCP 字节流
应用层协议
读缓冲
写缓冲
```

### Buffer 负责

```text
append 数据
retrieve 数据
readFd 从 socket 读
支持 [4字节长度][body] 解包
```

### 文件

```text
muduo/net/Buffer.h
muduo/net/Buffer.cc
```

### 最小接口

```cpp
class Buffer {
public:
    size_t readableBytes() const;
    const char* peek() const;

    void retrieve(size_t len);
    std::string retrieveAsString(size_t len);
    void append(const char* data, size_t len);

    ssize_t readFd(int fd, int* savedErrno);

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};
```

### 验收

写一个 length-header echo：

```text
client 发 [len][body]
server 能正确拆出完整 body
```

哪怕一次 recv 半包/多个包也正确。

---

## 阶段 8：封装 TcpConnection

### 学习点

```text
连接对象
回调设计
生命周期
读写缓冲区
```

### TcpConnection 负责

```text
管理一个 client_fd
拥有 Channel
拥有 inputBuffer/outputBuffer
处理 read/write/close/error
对外暴露 send()
```

### 文件

```text
muduo/net/TcpConnection.h
muduo/net/TcpConnection.cc
```

### 最小接口

```cpp
class TcpConnection {
public:
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*)>;

    TcpConnection(EventLoop* loop, int sockfd);
    ~TcpConnection();

    void send(const std::string& message);
    void shutdown();

    void setConnectionCallback(ConnectionCallback cb);
    void setMessageCallback(MessageCallback cb);

    void connectEstablished();
    void connectDestroyed();

private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    EventLoop* loop_;
    int sockfd_;
    Channel channel_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
};
```

### 验收

业务层只写：

```cpp
void onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
    std::string msg = buf->retrieveAllAsString();
    conn->send(msg);
}
```

---

## 阶段 9：封装 TcpServer

### 学习点

```text
server 管理所有连接
连接生命周期
map
shared_ptr
```

### TcpServer 负责

```text
持有 Acceptor
新连接来时创建 TcpConnection
保存到 connections_
连接断开时移除
对外设置回调
```

### 文件

```text
muduo/net/TcpServer.h
muduo/net/TcpServer.cc
```

### 最小接口

```cpp
class TcpServer {
public:
    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& name);

    void start();

    void setConnectionCallback(ConnectionCallback cb);
    void setMessageCallback(MessageCallback cb);

private:
    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    Acceptor acceptor_;
    std::unordered_map<std::string, TcpConnectionPtr> connections_;
};
```

### 验收

最终 echo server：

```cpp
int main() {
    EventLoop loop;
    InetAddress addr(9000);

    TcpServer server(&loop, addr, "EchoServer");
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer* buf) {
        conn->send(buf->retrieveAllAsString());
    });

    server.start();
    loop.loop();
}
```

---

## 阶段 10：加入多线程 EventLoopThreadPool

### 学习点

```text
std::thread
mutex
condition_variable
one loop per thread
跨线程任务投递
eventfd
```

### 任务

实现：

```text
main loop 负责 accept
sub loops 负责 client IO
新连接轮询分配给 sub loop
```

### 文件

```text
muduo/net/EventLoopThread.h
muduo/net/EventLoopThread.cc
muduo/net/EventLoopThreadPool.h
muduo/net/EventLoopThreadPool.cc
```

### 验收

设置：

```cpp
server.setThreadNum(4);
```

多 client 连接会分布到不同 IO 线程。

---

## 阶段 11：加入 TimerQueue

### 学习点

```text
timerfd
定时器
心跳
超时关闭
```

### 任务

支持：

```cpp
loop.runAfter(5.0, cb);
loop.runEvery(1.0, cb);
```

可做：

```text
连接 30 秒无消息自动关闭
```

---

## 阶段 12：基于 mini-muduo 做 mini-rpc

### 学习点

```text
协议设计
request_id
服务注册
序列化
异步回调
```

### RPC 包格式

```text
[4字节长度]
[4字节 request_id]
[2字节 service_id]
[2字节 method_id]
[body]
```

### 功能

```text
RpcServer 注册 service/method handler
RpcClient call(method, request, callback)
request_id 匹配 response
超时处理
```

---

## 学习节奏建议

```text
第 1 周：socket + epoll 原始版
第 2 周：Channel + Poller + EventLoop
第 3 周：Acceptor + Buffer + TcpConnection
第 4 周：TcpServer + 多线程
第 5 周：Timer + RPC
```

---

## 每阶段文档记录

每完成一阶段，在 README 记录：

```text
我实现了什么
遇到什么问题
关键 API 是什么
怎么测试
下一步是什么
```

这对复盘和求职都很有用。

---

## 最重要原则

不要一开始直接抄 muduo。

推荐顺序：

```text
阻塞 socket
→ epoll 原始版
→ Channel
→ Poller
→ EventLoop
→ Acceptor
→ Buffer
→ TcpConnection
→ TcpServer
→ 多线程
→ RPC
```

每个抽象都应该是从前一阶段的问题里自然抽出来的。
