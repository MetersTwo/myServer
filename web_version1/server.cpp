#include "server.h"

int initListenFd(){
    // socket
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
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
    uint16_t port = 1316;
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

int epollRun(int lfd){
    // epoll_create
    int epfd = epoll_create(1);
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
    printf("epoll success!\n"); // log
    struct epoll_event evs[1024];
    while(1){
        int size = sizeof(evs) / sizeof(struct epoll_event);
        int num = epoll_wait(epfd, evs, size, -1);
        for(int i = 0; i < num; ++i){
            int fd = evs[i].data.fd;
            if(fd == lfd){
                // accept
                acceptClient(lfd, epfd);
            }else{
                // deal read
                recvHttpRequest(fd, epfd);
            }
        }
    }
    return 1;
}

int acceptClient(int lfd, int epfd){
    // accept
    int cfd = accept(lfd, NULL, NULL);
    if(cfd == -1){
        perror("accept");
        return -1;
    }
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

    return 1;
}

int recvHttpRequest(int cfd, int epfd){
    int len = 0;
    int total = 0;
    char buf[4096] = {0};
    char tmp[1024] = {0};
    while((len = recv(cfd, tmp, sizeof tmp, 0)) > 0){
        if(total + len >= sizeof buf){
            return -1;
        }
        memcpy(buf + total, tmp, len);
        total += len;
    }
    // recv finish ? 
    if(len == -1 && errno == EAGAIN){
        // parse request line
        char *pt = strstr(buf, "\r\n");
        int reqLen = pt - buf;
        buf[reqLen] = '\0';
        parseRequestLine(buf, cfd);
    }else if(len == 0){
        epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
        close(cfd);
    }else{
        perror("recv");
        return -1;
    }
    return 1;
}

int parseRequestLine(const char* line, int cfd){
    // get /xx/1.jpg
    char method[4];
    char path[1024];
    sscanf(line, "%[^ ] %[^ ]", method, path);
    printf("method: %s path: %s\n", method, path); // log
    if(strcasecmp(method, "get") != 0){
        return -1;
    }
    char* file = NULL;
    if(strcmp(path, "/") == 0){
        file = "./";
    }else{
        file = path + 1;
    }

    printf("cmp :%s || %s\n", path, file);

    struct stat st;
    int ret = stat(file, &st);
    printf("file: %s, ret val: %d\n", file, ret);
    if(ret == -1){
        // 回复404
        sendHeadMsg(cfd, 404, "Not found", getFileType(".html"), -1);
        sendFile("/home/lin/404.html", cfd);
        perror("stat");
        return 0;
    }

    if(S_ISDIR(st.st_mode)){
        // 发送目录内容
        sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
        sendDir(file, cfd);
    }else{
        // 发送文件内容
        sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
        sendFile(file, cfd);
    }

    return 1;
}

int sendFile(const char *fileName, int cfd){
    printf("sendfile: %s\n", fileName);
    int fd = open(fileName, O_RDONLY);
    assert(fd > 0);
#if 1
    while(true){
        char buf[1024];
        int len = read(fd, buf, sizeof buf);
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
    int size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    sendfile(cfd, fd, NULL, size);
#endif
    close(fd);
    return 1;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length){
    char buf[4096] = {0};
    sprintf(buf, "http/1.1 %d %s\r\n", status, descr);
    sprintf(buf + strlen(buf), "content-type: %s\r\n", type);
    sprintf(buf + strlen(buf), "content-length: %d\r\n\r\n", length);
    
    send(cfd, buf, strlen(buf), 0);
    return 1;
}

int sendDir(const char *dirName, int cfd){
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

const char *getFileType(const char *name){
    const char *dot = strrchr(name, '.');
    if(dot == NULL)
        return "text/plain; charset=utf-8";
    if(strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if(strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";    
    if(strcmp(dot, ".gif") == 0)
        return "image/gif";
    if(strcmp(dot, ".png") == 0)
        return "image/png";
    if(strcmp(dot, ".css") == 0)
        return "text/css";
    if(strcmp(dot, ".au") == 0)
        return "audio/basic";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    // if(strcmp(dot, ".wav") == 0)
    //     return "image/gif";
    return "text/plain; charset=utf-8";      
}