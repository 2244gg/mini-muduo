#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>

int setNonBlocking(int fd){
    int flags=fcntl(fd,F_GETFL,0);

    if(flags<0){
        return -1;
    }
    return fcntl(fd,F_SETFL,flags|O_NONBLOCK);
}

int main(){
    int listen_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    setNonBlocking(listen_fd);

    int opt=1;

    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(9000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listen_fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));

    listen(listen_fd,128);

    int epoll_fd=epoll_create1(0);

    epoll_event ev{};
    ev.data.fd=listen_fd;
    ev.events=EPOLLIN;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&ev);

    std::cout<<"epoll echo server listen on port 9000\n";

    epoll_event events[1024];

    while(true){
        int n = epoll_wait(epoll_fd,events,1024,-1);
        if(n<0){
            if(errno==EINTR){
                continue;
            }
            std::cerr<<"epoll_wait failed\n";
            break;
        }
        for(int i=0;i<n;i++){
            int fd=events[i].data.fd;
            uint32_t event=events[i].events;
            if(event&(EPOLLERR|EPOLLHUP)){
                std::cerr<<"fd error or hangup\n";
                epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd, NULL);
                close(fd);
                continue;
            }
            if(fd==listen_fd){
                while(true){
                    sockaddr_in client_addr{};
                    socklen_t client_len=sizeof(client_addr);
                    int client_fd=accept(listen_fd,reinterpret_cast<sockaddr*>(&client_addr),&client_len);
                    if(client_fd<0){
                        if(errno==EAGAIN||errno==EWOULDBLOCK){
                            break;
                        }
                        std::cout<<"accept failed\n";
                        break;
                    }
                    setNonBlocking(client_fd);
                    epoll_event ev{};
                    ev.data.fd=client_fd;
                    ev.events=EPOLLIN;
                    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&ev);
                }
            }else{
                char buf[1024];
                while(true){
                    ssize_t bytes=recv(fd,buf,sizeof(buf),0);
                    if(bytes>0){
                        int sent=send(fd,buf,bytes,0);
                        if(sent<0){
                            std::cerr<<"send failed\n";
                            break;
                        }
                    }else if(bytes==0){
                        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
                        close(fd);
                        std::cout<<"disconnected\n";
                        break;
                    }else{
                        if(errno==EAGAIN||errno==EWOULDBLOCK){
                            break;
                        }
                        std::cerr<<"recv failed\n";
                        epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL);
                        close(fd);
                        break;
                    }
                }
            }
        }
    }
    close(listen_fd);
    close(epoll_fd);
    return 0;
}