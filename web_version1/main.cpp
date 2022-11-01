#include <iostream>
#include "server.h"
using namespace std;

int main(){

    // 切换服务器的工作目录
    chdir("/home/lin/web");
    //初始化监听套接字
    int lfd = initListenFd();
    // 启动服务
    epollRun(lfd);
    return 0;
}