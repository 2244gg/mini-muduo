#include "Channel.h"
#include <sys/epoll.h>
Channel::Channel(int fd)
{
    fd_=fd;
}

int Channel::fd() const{
    return fd_;
}

uint32_t Channel::events() const{
    return events_;
}
uint32_t Channel::set_revents(uint32_t revt){
    revents_=revt;
    return revents_;
}

void Channel::setReadCallback(EventCallback cb){
    readCallback_=cb;
}
void Channel::setWriteCallback(EventCallback cb){
    writeCallback_=cb;
}
void Channel::setCloseCallback(EventCallback cb){
    closeCallback_=cb;
}
void Channel::setErrorCallback(EventCallback cb){
    errorCallback_=cb;
}

void Channel::handleEvent(){
    if(events_&revents_==0) return;
    switch(revents_){
        case EPOLLIN:
            readCallback_();
        break;
        case EPOLLOUT:
            writeCallback_();
        break;
        case EPOLLHUP:
            closeCallback_();
        break;
        case EPOLLERR:
            errorCallback_();
        break;
    }
}