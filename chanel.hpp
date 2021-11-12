#include <boost/fiber/buffered_channel.hpp>
#include <atomic>

template <typename T>
class Chanel {
    std::atomic<bool> closed = false;

public:
    friend void operator<<(T& out, Chanel<T>& chan) {
        chan.closed.store(!chan.vGetValue(out));
    }
    friend void operator>>(T&& in, Chanel<T>& chan) {
        chan.vPutValue(std::move(in));
    }
    explicit operator bool() {
        return !closed;
    }
    virtual void Close() {
        vClose();
    }

private:
    virtual bool vGetValue(T& out) = 0;
    virtual void vPutValue(T&& in) = 0;
    virtual void vClose() = 0;
};

template <typename T>
class FiberChanel : public Chanel<T>{
    boost::fibers::buffered_channel<T> chan;

public:
    FiberChanel(unsigned int capacity) : chan(capacity)
    {}

private:
    bool vGetValue(T& out) override {
        return chan.pop(out) != boost::fibers::channel_op_status::success ? false : true;
    }

    void vPutValue(T&& in) override {
        chan.push(in);
    }

    void vClose() override {
        chan.close();
    }
};