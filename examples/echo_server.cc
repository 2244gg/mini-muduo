#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <unistd.h>

int main(){
    //std::cout<<"hello world\n";
    int listen_fd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(listen_fd<0){
        std::cerr<<"socket failed\n";
    }
    int opt = 1;
    setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);

    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    int ret=bind(listen_fd,reinterpret_cast<sockaddr*>(&server_addr),sizeof(server_addr));

    ret=listen(listen_fd,128);
    if(ret<0){
        std::cerr<<"listen failed\n";
        close(listen_fd);
        return 1;
    }

    std::cout<<"server listen on port 9000\n";

    while(true){
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd=accept(listen_fd,reinterpret_cast<sockaddr*>(&client_addr),&client_len);
        if(client_fd<0){
            std::cerr<<"accept failed\n";
            continue;
        }
        std::cout<<"new client connected\n";
        char buf[1024];
        while(true){
            size_t n=recv(client_fd,buf,sizeof(buf),0);
            if(n>0){
                std::cout<<"recv"<<n<<"bytes\n";
                send(client_fd,buf,n,0);
            }else if(n==0){
                std::cout<<"client closed\n";
                break;
            }else{
                std::cerr<<"recv failed\n";
            }
        }
        close(client_fd);
        
    }
    close(listen_fd);
    return 0;
}