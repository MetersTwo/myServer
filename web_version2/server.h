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
#include <time.h>

class Client{
public:
    Client(int _cfd):cfd(_cfd),isClosed(false){}
    Client() = default;
public:
    // 初始化Client对象
    void init(int epfd_, int cfd_);
    // 接收http请求
    int recvHttpRequest();
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
    int onRead();
    // 关闭
    int C_close();

private:
    bool isClosed;
    int cfd, epfd;
    static std::unordered_map<std::string, std::string> fileType;
    std::unordered_map<std::string, std::string> rqHead;
    std::string rbuf;
};

// typedef std::chrono::high_resolution_clock Clock;
// typedef std::chrono::milliseconds MS;

struct TimeNode{
    // std::chrono::time_point<std::chrono::high_resolution_clock> t;
    time_t expire;
    std::function<void()> cb_func;
    int fd;
    bool operator<(const TimeNode& tn){
        return this->expire < tn.expire;
    }
    TimeNode(){}
    TimeNode(time_t exptime,std::function<void()> func, int fd):expire(exptime), cb_func(func), fd(fd){}
};

class Time_heap{
public:
    Time_heap(int timeOut = 5):timeout_ms(timeOut){
        timeList.reserve(10);
    }
    Time_heap() = default;
public:
    int push(time_t exp_time,std::function<void()> func,int fd);
    int push(TimeNode tn);

    int pop();
    int getNextTime();
    int tick();
    inline bool empty(){return timeList.empty();}
    void test_show_arr(const std::string str);

private:
    void swp(int a, int b);
    void swp(TimeNode& a, TimeNode& b);
    int dwnAdj(int idx);
    int upAdj(int idx);
    std::vector<TimeNode> timeList; 
    int timeout_ms;
};

class Server{
public:
    Server(int _port = 1316):port(_port), timer(5){}
    // 初始化监听套接字
    int initListenFd(); 
    // 启动epoll
    int epollRun();     
    // 和客户端建立连接
    int acceptClient(); 
    // 开始工作
    void run();
    // 关闭连接
    void S_close(int cfd);
    // 处理读事件
    void S_onRead(int fd);
private:
    int lfd;
    int epfd;
    uint16_t port;
    std::unordered_map<int, Client> clients;
    Time_heap timer;
};

#endif