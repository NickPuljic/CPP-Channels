#ifndef CHAN_H
#define CHAN_H

#include "buffer.h"

#include <functional>
#include <exception>

// for simplicity, we inherit std::exception and not std::runtime_error for now.
class ChannelClosedDuringSendException : public std::exception {
    const char* what() const noexcept override;
};

const char* ChannelClosedDuringSendException::what() const noexcept {
    return "while waiting to send, the channel was closed by another thread";
}

class ChannelClosedDuringRecvException : public std::exception {
    const char* what() const noexcept override;
};

const char* ChannelClosedDuringRecvException::what() const noexcept {
    return "while waiting to recv, the channel was closed by another thread";
}

class ChannelDestructedDuringSendException : public std::exception {
    const char* what() const noexcept override;
};

const char* ChannelDestructedDuringSendException::what() const noexcept {
    return "while waiting to send, the channel was destructed";
}

class ChannelDestructedDuringRecvException : public std::exception {
    const char* what() const noexcept override;
};

const char* ChannelDestructedDuringRecvException::what() const noexcept {
    return "while waiting to recv, the channel was destructed";
}

class SendOnClosedChannelException : public std::exception {
    const char* what() const noexcept override;
};

const char* SendOnClosedChannelException::what() const noexcept {
    return "send on closed channel";
}

class CloseOfClosedChannelException : public std::exception {
    const char* what() const noexcept override;
};

const char* CloseOfClosedChannelException::what() const noexcept {
    return "close of closed channel";
}

template<typename T>
class ChanData {
private:
    // data buffer for buffered channels.
    Buffer<T> buffer;
    
    // queues for waiting senders.
    // sender creates a future-promise pair, pushes the promise to send_queue, 
    // and waits until it get_value() from the future.
    // a receiver pops from the send_queue, and set_value() the promise,
    // thereby waking up the waiting sender.
    std::queue<std::promise<void>> send_queue;

    // sender passes the data of type T to the receiver thru sender_data_queue.
    std::queue<T> send_data_queue;

    // same logic as send_promise_queue,
    // but the data of type T is sent via promise, not a separate queue.
    std::queue<std::promise<T>> recv_queue;

    // is_closed is atomic to enable lock-free fast-track condition in chan_recv.
    // note that assignment and operator= on cur_size are atomic.
    std::atomic<bool> is_closed{false};
    
    std::mutex chan_lock;

    bool chan_send(const T& src, bool is_blocking);
    std::pair<bool, bool> chan_recv(T& dst, bool is_blocking);

public:
    explicit ChanData(size_t n = 0);

    // destructor is required to release the promises before destruction of queues.
    // while user definition of Destructor calls for user definition of copy and move,
    // we forgo because only the Chan wrapper class should access ChanData,
    // and therefore copy and move are never called.
    ~ChanData();

    // blocking send (ex. chan <- 1) does not return a boolean
    void send(const T& src);

    // return value indicates whether the communication succeeded
    // the value is true if the value received was delivered by a successful send operation to the channel,
    // or false if it is a zero value generated because the channel is closed and empty
    bool recv(T& dst);
    // Assignment recv
    T recv();

    // non-blocking versions of send and recv.
    // we expose the non-blocking versions to the user,
    // who can combine them in if/else block to simulate the select stmt.
    // the return values indicate whether the send or recv was successful.
    bool send_nonblocking(const T& src);
    bool recv_nonblocking(T& dst);

    // for-each semantics
    void foreach(std::function<void(T)> f);

    // Prevent sending to the channel
    void close();

};

template<typename T>
ChanData<T>::ChanData(size_t n) : buffer(n) {};

template<typename T>
ChanData<T>::~ChanData() {
    // release all receivers.
    // unlike ChannelClosedDuringRecvException, 
    // ChannelDestructedDuringRecvException is rethrown by the receiver.
    while (!recv_queue.empty()) {
        recv_queue.front().set_exception(std::make_exception_ptr(ChannelDestructedDuringRecvException()));
        recv_queue.pop();
    }

    // release all senders.
    while (!send_queue.empty()) {
        send_queue.front().set_exception(std::make_exception_ptr(ChannelDestructedDuringSendException()));
        send_queue.pop();
        send_data_queue.pop();
    }
}

template<typename T>
void ChanData<T>::send(const T& src) {
    chan_send(src, true);
}

template<typename T>
T ChanData<T>::recv() {
    T temp;
    recv(temp);
    return temp;
}

template<typename T>
bool ChanData<T>::recv(T& dst) {
    std::pair<bool, bool> selected_received = chan_recv(dst, true);
    return selected_received.second;
}

template<typename T>
bool ChanData<T>::send_nonblocking(const T& src) {
    return chan_send(src, false);
}

template<typename T>
bool ChanData<T>::recv_nonblocking(T& dst) {
    std::pair<bool, bool> selected_received = chan_recv(dst, false);
    return selected_received.first;
}

template<typename T>
bool ChanData<T>::chan_send(const T& src, bool is_blocking) {
    // Fast path: check for failed non-blocking operation without acquiring the lock.
    if (!is_blocking
        && !is_closed
        && ((buffer.capacity() == 0 && recv_queue.empty()) || (buffer.capacity() > 0 && buffer.is_full())))
        return false;

    // scoped_lock can't be used b/c we must .unlock() prior to future.get()
    std::unique_lock<std::mutex> lck{chan_lock};

    // sending to a closed channel is an error.
    if (is_closed) {
        throw SendOnClosedChannelException();
    }

    // if a waiting receiver exists,
    // pass the value we want to send directly to the receiver,
    // bypassing the buffer (if any).
    if (!recv_queue.empty()) {
        recv_queue.front().set_value(src);
        recv_queue.pop();
        return true;
    }

    // if space is available in the buffer, enqueue the element to send.
    if (!buffer.is_full()) {
        buffer.push(src);
        return true;
    }

    // if not blocking (select stmt), return false.
    if (!is_blocking) {
        return false;
    }

    // block on the channel. Some receiver will complete our operation for us.
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    send_queue.push(std::move(promise));
    send_data_queue.push(src);

    lck.unlock();

    // if close() passes an exception, rethrow to user.
    future.get();

    return true;
}

// receives on channel c and writes the received data to dst.
// if not blocking and no elements are available, returns (false, false).
// else if c is closed, zeros dst and returns (true, false).
// else, fills in dst with an element and returns (true, true).
// A non-nil dst must refer to the heap or the caller's stack.
// two bools in a pair are (selected, received).
template<typename T>
std::pair<bool, bool> ChanData<T>::chan_recv(T& dst, bool is_blocking) {
    // from chan.go:
    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // The order of operations is important here: reversing the operations can lead to
    // incorrect behavior when racing with a close.
    // Note that current_size and is_closed are each read atomically,
    // but they need not be read together atomically.
    // i.e. current_size may have been modified before reading is_closed.
    if (!is_blocking
        && ((buffer.capacity() == 0 && send_queue.empty()) || (buffer.capacity() > 0 && buffer.current_size() == 0))
        && !is_closed) {
        return std::pair<bool, bool>(false, false);
    }

    std::unique_lock<std::mutex> lck{chan_lock};

    // else if c is closed, returns (true, false).
    if (is_closed && buffer.current_size() == 0) {
        return std::pair<bool, bool>(true, false);
    }

    // from chan.go:
    // Found a waiting sender. If buffer is size 0, receive value directly from sender.
    // Otherwise, receive from head of buffer and add sender's value to the tail of the buffer.
    // (both map to the same buffer slot because the queue (buffer) is full,
    // i.e. if buffer was not full, no sender would be waiting)
    if (!send_queue.empty()) {
        if (buffer.capacity() == 0) {
            dst = send_data_queue.front();
        } else {
            dst = buffer.front();
            buffer.pop();
            buffer.push(send_data_queue.front());
        }
        send_queue.front().set_value(); // sender is unblocked.
        send_data_queue.pop();
        send_queue.pop();
        return std::pair<bool, bool>(true, true);
    }

    // if buffer is not empty, recv from buffer.
    if (buffer.current_size() > 0) {
        dst = buffer.front();
        buffer.pop();
        return std::pair<bool, bool>(true, true);
    }

    // if not blocking (select stmt), return false.
    if (!is_blocking) {
        return std::pair<bool, bool>(false, true);
    }

    // block on the channel.
    std::promise<T> promise;
    std::future<T> future = promise.get_future();
    recv_queue.push(std::move(promise));

    lck.unlock();

    try {
        // if close() passes exception, dst is not set, because future.get() throws the exception.
        dst = future.get();
    } 
    catch (ChannelClosedDuringRecvException) {
        // ignore exception passed by close(), b/c !is_closed indicator is returned to user.
    }

    return std::pair<bool, bool>(true, !is_closed);
}

template<typename T>
void ChanData<T>::foreach(std::function<void(T)> f){
    T cur_data;
    bool received = recv(cur_data);
    while (received) {
        f(cur_data);
        received = recv(cur_data);
    }
}

template<typename T>
void ChanData<T>::close(){
    std::unique_lock<std::mutex> lck{chan_lock};

    if (is_closed) {
        throw CloseOfClosedChannelException();
    }

    is_closed = true;

    // release all receivers.
    while (!recv_queue.empty()) {
        // the waiting receiver should not throw this exception.
        recv_queue.front().set_exception(std::make_exception_ptr(ChannelClosedDuringRecvException()));
        recv_queue.pop();
    }

    // release all senders.
    // By Go semantics, senders should throw exception to users.
    while (!send_queue.empty()) {
        // the waiting sender should rethrow this exception.
        send_queue.front().set_exception(std::make_exception_ptr(ChannelClosedDuringSendException()));
        send_queue.pop();
        send_data_queue.pop();
    }
}

template<typename T>
class Chan {
private:
    std::shared_ptr<ChanData<T>> chan_data_shared_ptr;
public:
    Chan(size_t n = 0) : chan_data_shared_ptr(new ChanData<T>(n)) {}
    
    // rule of 5.
    // default method calls that of std::shared_ptr.
    ~Chan()                                 = default;
    Chan(const Chan& c)                     = default;
    Chan& operator=(const Chan& c)          = default;
    Chan(Chan&& c)                          = default;
    Chan& operator=(Chan&& c)               = default;

    // forward methods
    void send(const T& src)                 {chan_data_shared_ptr->send(src);}
    bool recv(T& dst)                       {return chan_data_shared_ptr->recv(dst);}
    T recv()                                {return chan_data_shared_ptr->recv();}
    bool send_nonblocking(const T& src)     {return chan_data_shared_ptr->send_nonblocking(src);}
    bool recv_nonblocking(T& dst)           {return chan_data_shared_ptr->recv_nonblocking(dst);}
    void foreach(std::function<void(T)> f)  {chan_data_shared_ptr->foreach(f);}
    void close()                            {chan_data_shared_ptr->close();}
};

#endif
