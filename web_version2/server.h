#ifndef _SERVER_H_
#define _SERVER_H

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <vector>
#include <stdio.h>

class Client{
public:
    Client(int _cfd):cfd(_cfd){}
    Client() = default;
    // 接收http请求
    int recvHttpRequest(int epfd);
    // 解析HTTP请求
    int parse(const char* line);
    // 解析请求行
    int parseRequestLine(const char* line);
    // 解析请求头
    // 解析请求体

    // 发送文件
    int sendFile(const char *fileName);
    // 发送(状态行+响应头)
    int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
    // 寻找格式匹配的content-type
    std::string getFileType(const char *name);
    // 发送目录
    int sendDir(const char *dirName);
    // 处理输入
    int onRead(int epfd);
private:
    int cfd;
    static std::unordered_map<std::string, std::string> fileType;
    std::unordered_map<std::string, std::string> rqHead;
    std::string rbuf;
};

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;

struct TimeNode{
    std::chrono::time_point<std::chrono::high_resolution_clock> t;
    std::function<void()> cb_func;
    int epfd;
    int fd;
    bool operator<(const TimeNode &b){
        return t < b.t;
    }
    bool operator>(const TimeNode &b){
        return t > b.t;
    }
    TimeNode(){}
    TimeNode(
    std::chrono::time_point<std::chrono::high_resolution_clock> t,
    std::function<void()> func,
    int epfd,
    int fd):
    t(t),
    cb_func(func),
    epfd(epfd),
    fd(fd){}
};

class Time_heap{
public:
    Time_heap(int timeOut = 60000):timeout_ms(timeOut){}
    Time_heap() = default;
    void swp(int a, int b);
    void swp(TimeNode& a, TimeNode& b);
    int dwnAdj(int idx);
    int upAdj(int idx);
    int push(std::chrono::time_point<std::chrono::high_resolution_clock> y,
    std::function<void()>,
    int epfd,
    int fd);
    int push(TimeNode tn);
    int pop();
    int getNextTime();
    int tick();
    inline bool empty(){return timeList.empty();}
private:
    std::vector<TimeNode> timeList; 
    int timeout_ms;
};

class Server{
public:
    Server(int _port = 1316):port(_port),timer(10000){}
    // 初始化监听套接字
    int initListenFd(); 
    // 启动epoll
    int epollRun();     
    // 和客户端建立连接
    int acceptClient(); 
    // 开始工作
    void run();
    // 关闭连接
    void closefd(int cfd);
private:
    int lfd;
    int epfd;
    uint16_t port;
    std::unordered_map<int, Client> clients;
    Time_heap timer;
};

#endif