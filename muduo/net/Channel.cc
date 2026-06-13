#include "Channel.h"
#include <sys/epoll.h>
Channel::Channel(int fd,uint32_t events=0)
{
    fd_=fd;
    events_=events;
}

int Channel::fd() const{
    return fd_;
}

uint32_t Channel::events() const{
    return events_;
}
void Channel::set_revents(uint32_t revt){
    revents_=revt;
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
    if((events_&revents_)==0) return;
    if(revents_&EPOLLERR){
        errorCallback_();
    }
    if((revents_&EPOLLHUP)&&!(revents_&EPOLLIN)){
        closeCallback_();
    }
    if(revents_&EPOLLIN){
        readCallback_();
    }
    if(revents_&EPOLLOUT){
        writeCallback_();
    }
}