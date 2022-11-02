#include "server.h"

using namespace std;

int Server::initListenFd(){
    // socket
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1){
        perror("socket");
        return -1;
    }
    // setsocket reuseaddr
    int opt = 1;
    int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret == -1){
        perror("setsocket");
        return -1;
    }
    // bind 
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind(lfd, (struct sockaddr*) &addr, sizeof addr);
    if(ret == -1){
        perror("bind");
        return -1;
    }
    // listen
    ret = listen(lfd, 128);
    if(ret == -1){
        perror("listen");
        return -1;
    }
    // return listenfd
    return lfd;
}

int Server::epollRun(){
    // epoll_create
    epfd = epoll_create(1);
    if(epfd == -1){
        perror("epoll_create");
        return -1;
    }
    // epoll_ctl add
    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    struct epoll_event evs[1024];
    while(1){
        int timeout = timer.getNextTime() * 1000;
        int num = epoll_wait(epfd, evs, 1000, timeout);
        for(int i = 0; i < num; ++i){
            int fd = evs[i].data.fd;
            uint32_t event = evs[i].events;
            if(fd == lfd){
                // accept
                acceptClient();
            }else if(event & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)){
                S_close(fd);
            }else {
                // deal read
                S_onRead(fd);
            }
        }
        timer.tick();
    }
    return 1;
}

int Server::acceptClient(){
    // accept
    int cfd = accept(lfd, NULL, NULL);
    if(cfd == -1){
        perror("accept");
        return -1;
    }
    printf("[accept]: %d\n", cfd);
    // set noneblock
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);
    // add cfd
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if(ret == -1){
        perror("epoll_ctl");
        return -1;
    }
    // 初始化Client对象
    clients[cfd].init(epfd, cfd);

    time_t curr = time(NULL);
    timer.push(curr, bind(&Server::S_close, this, cfd), cfd);
    
    return 0;
}

void Server::run(){
    // 切换服务器的工作目录
    // chdir("/home/lin/web");
    chdir("/home/lin/web/resources/images/");

    //初始化监听套接字
    lfd = initListenFd();
    // 启动服务
    epollRun();
}

void Server::S_close(int cfd){
    // 懒关闭
    clients[cfd].C_close();
    return;
}

void Server::S_onRead(int cfd){
    clients[cfd].onRead();
}


/*
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
*/
void Client::init(int epfd_, int cfd_){
    isClosed = false;
    epfd = epfd_;
    cfd = cfd_;
    if(!rbuf.empty())
        rbuf.clear();
    return;
}

int Client::recvHttpRequest(){
    int len = 0;
    int total = 0;
    
    char tmp[1024] = {0};
    // 从缓冲区读取
    while((len = recv(cfd, tmp, sizeof tmp, 0)) > 0){
        rbuf += string(tmp);
    }
    printf("[%d] recv HTTP: {\n%s}\n", cfd, rbuf.c_str()); // log
    
    // 解析输入的报文 
    if(len == -1 && errno == EAGAIN){
        int cut = rbuf.find("/r/n");
        parse(rbuf.substr(0, cut).c_str());

    // }else if(len == 0){
    //     // epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    //     // close(cfd);
    //     // C_close(epfd);
    //     // printf("client close!\n");
    //     return 0;
    }else{
        perror("recv");
        return -1;
    }
    return 1;
}

int Client::parse(const char* line){
    parseRequestLine(line);
    // printf("after respond close fd!\n");
    C_close();
    return 0;
}

int Client::parseRequestLine(const char* line){
    // get /xx/1.jpg
    char method[4];
    char path[1024];
    sscanf(line, "%[^ ] %[^ ]", method, path);
    printf(" [method]: %s\n [path]: %s\n", method, path); // log
    if(strcasecmp(method, "get") != 0){
        return -1;
    }
    char* file = NULL;
    if(strcmp(path, "/") == 0){
        file = "./";
    }else{
        file = path + 1;
    }

    struct stat st;
    int ret = stat(file, &st);
    printf("file: %s, ret val: %d\n", file, ret);
    if(ret == -1){
        // 回复404
        sendHeadMsg(cfd, 404, "Not found", getFileType(".html").c_str(), -1);
        sendFile("/home/lin/404.html");
        perror("stat");
        return 0;
    }

    if(S_ISDIR(st.st_mode)){
        // 发送目录内容
        sendHeadMsg(cfd, 200, "OK", getFileType(".html").c_str(), -1);
        sendDir(file);
    }else{
        // 发送文件内容
        sendHeadMsg(cfd, 200, "OK", getFileType(file).c_str(), st.st_size);
        sendFile(file);
    }

    printf("\n}\n");    // log

    return 1;
}

int Client::sendFile(const char *fileName){
    printf("sendfile: %s\n", fileName);
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
#if 0
    while(true){
        char buf[1024];
        int len = read(fd, buf, 1000);
        if(len > 0){
            send(cfd, buf, len, 0);
            usleep(10);// 这非常重要
        }else if(len == 0){
            break;
        }else{
            perror("read");
            return -1;
        }
    }
#else 
    off_t offset;
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    sendfile(cfd, fd, NULL, size);
    // while(offset < size){
    //     int ret = sendfile(cfd, fd, &offset, size - offset);
    //     if(ret != -1) printf("have send %dbytes\n", ret);
    // }
#endif
    close(fd);
    return 1;
}

int Client::sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length){
    char buf[4096] = {0};
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "Content-length: %d\r\n", length);
    sprintf(buf + strlen(buf), "Connection: keep-alive\r\n\r\n");

    printf("[%d] respond HTTP: {\n%s", cfd, buf); // log
    send(cfd, buf, strlen(buf), 0);
    return 1;
}

int Client::sendDir(const char *dirName){
    char buf[4096];
    sprintf(buf, "<html><head><title>%s</title></head><body><table>", dirName);
    struct dirent **namelist;
    int num = scandir(dirName, &namelist, NULL, alphasort);
    for(int i = 0; i < num; ++i){
        // get filename
        char *name = namelist[i]->d_name;
        char tmp[1024];
        sprintf(tmp, "%s/%s", dirName, name);
        struct stat st;
        stat(tmp, &st);
        if(S_ISDIR(st.st_mode)){
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }else{
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", name, name, st.st_size);
        }
        send(cfd, buf, strlen(buf), 0);
        memset(buf, 0, sizeof(buf));
        free(namelist[i]);
    }
    sprintf(buf, "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);
    free(namelist);

    return 0;
}

std::unordered_map<std::string, std::string> Client::fileType = {
    {".html"  ,"text/html; charset=utf-8"   },
    {".htm"   ,"text/html; charset=utf-8"   },
    {".jpg"   ,"image/jpeg"                 },
    {".jpeg"  ,"image/jpeg"                 },
    {".gif"   ,"image/gif"                  },
    {".png"   ,"image/png"                  },
    {".css"   ,"text/css"                   },
    {".au"    ,"audio/basic"                },
    {"default", "text/plain; charset=utf-8" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".js",    "text/javascript "},
};

string Client::getFileType(const char *_name){
    std::string name = _name;
    int dotPos = name.find('.');
    if(dotPos == std::string::npos){
        return "text/plain; charset=utf-8";
    }
    std::string dot = name.substr(dotPos);
    if(fileType.find(dot) == fileType.end()){
        return fileType["default"];
    }
    return fileType[dot];    
}

int Client::onRead(){
    int ret = recvHttpRequest();
}

int Client::C_close(){
    // 懒关闭 不删除Client对象，而是将isClosed设置为true
    if(isClosed == false){
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        isClosed = true;
        close(cfd);
        rbuf.clear();
        printf("Close!\n");
    }
    return 0;
}

/*
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
***************************************************************************************************
*/
void Time_heap::swp(int a, int b){
    if(a == b) return;
    swp(timeList[a], timeList[b]);
    return;
}
void Time_heap::swp(TimeNode& a, TimeNode& b){
    swap(a, b);
    return;
}
int Time_heap::dwnAdj(int idx){
    int n = timeList.size();
    int l, r, tmp;
    while(idx*2+1 < n){
        tmp = idx;
        l = idx * 2 + 1, r = idx * 2 + 2;
        if(timeList[l] < timeList[tmp]){
            tmp = l;
        }
        if(r < n && timeList[r] < timeList[tmp]){
            tmp = r;
        }
        if(tmp != idx){
            swp(timeList[tmp], timeList[idx]);
        }else{
            break;
        }
        idx = tmp;
    }
    return 0;
}

int Time_heap::upAdj(int idx){
    while(idx > 0){
        int fa = (idx - 1) / 2;
        if(timeList[idx] < timeList[fa]){
            swp(idx, fa);
        }else{
            break;
        }
        idx = fa;
    }
    return 0;
}

int Time_heap::push(time_t exp_time, std::function<void()> func, int fd){
    test_show_arr("push working");
    int idx = timeList.size();
    timeList.emplace_back(exp_time, func, fd);
    upAdj(idx);
}

int Time_heap::push(TimeNode tn){
    test_show_arr("push working");
    int idx = timeList.size();
    timeList.push_back(tn);
    upAdj(idx);
    return 0;
}

int Time_heap::pop(){
    std::function<void()> f = timeList[0].cb_func;
    int idx = timeList.size()-1;
    swp(0, idx);
    timeList.pop_back();
    dwnAdj(0);
    f();
    return 0;
}

int Time_heap::getNextTime(){
    if(timeList.empty()) return -1;
    time_t curr = time(NULL);
    int ret = curr - timeList[0].expire;
    if(ret < 0) {
        ret = 0;
    }
    return ret;
}

int Time_heap::tick(){
    while(!timeList.empty()){
        time_t curr = time(NULL);
        int time = curr - timeList[0].expire;
        // printf("%ld\n", time);
        if(time < timeout_ms){
            break;
        }
        test_show_arr("tick working");
        pop();
    }
    return 0;
}

void Time_heap::test_show_arr(const std::string str){
    return;
    printf("%s: ", str.c_str());
    for(auto tmp : timeList){
        printf("[%d] ", tmp.fd);
    }
    printf("\n");
}