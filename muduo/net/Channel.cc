#include "Channel.h"
#include <sys/epoll.h>
Channel::Channel(int fd,uint32_t events)
{
    fd_=fd;
    events_=events;
    revents_=0;
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

void Channel::disableAll(){
    events_=0;
}
void Channel::enableReading(){
    events_=events_|EPOLLIN;
}

void Channel::handleEvent(){
    if(errorCallback_&&(revents_&EPOLLERR)){
        errorCallback_();
    }
    if(closeCallback_&&(revents_&EPOLLHUP)&&!(revents_&EPOLLIN)){
        closeCallback_();
    }
    if(readCallback_&&revents_&EPOLLIN){
        readCallback_();
    }
    if(writeCallback_&&revents_&EPOLLOUT){
        writeCallback_();
    }
}