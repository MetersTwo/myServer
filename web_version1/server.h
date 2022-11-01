#ifndef _SERVER_H_
#define _SERVER_H

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/sendfile.h>
#include <dirent.h>



class Server{
public:
    // 初始化监听套接字
    int initListenFd();
private:
    int lfd;
};

// 初始化监听套接字
int initListenFd();
// 启动epoll
int epollRun(int lfd);
// 和客户端建立连接
int acceptClient(int lfd, int epfd);
// 接收http请求
int recvHttpRequest(int cfd, int epfd);
// 解析请求行
int parseRequestLine(const char* line, int cfd);
// 发送文件
int sendFile(const char *fileName, int cfd);
// 发送(状态行+响应头)
int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
// 寻找格式匹配的content-type
const char *getFileType(const char *name);
// 发送目录
int sendDir(const char *dirName, int cfd);
#endif