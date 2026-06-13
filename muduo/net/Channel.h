#pragma once

#include <stdint.h>
#include <functional>

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(int fd,uint32_t events=0);

    int fd() const;

    uint32_t events() const;
    void set_revents(uint32_t revt);
    
    void setReadCallback(EventCallback cb);
    void setWriteCallback(EventCallback cb);
    void setCloseCallback(EventCallback cb);
    void setErrorCallback(EventCallback cb);

    void handleEvent();

    void disableAll();
    void enableReading();
private:
    int fd_;
    uint32_t events_;
    uint32_t revents_;

    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};
