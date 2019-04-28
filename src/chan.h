#ifndef CHAN_H
#define CHAN_H

#include "buffer.h"

#include <functional>

template<typename T>
class Chan {
private:
    Buffer<T> buffer;
    // queues for waiting senders and receivers, respectively
    // using a std::promise object, we pass a value,
    // that is acquired asynchronously by a corresponding std::future object
    std::queue<std::pair<std::promise<void>*, T>> send_queue;
    std::queue<std::promise<T>> recv_queue;

    // is_closed is atomic to enable lock-free fast-track condition in chan_recv.
    // note that assignment and operator= on cur_size are atomic
    std::atomic<bool> is_closed{false};
    std::mutex chan_lock;

    bool chan_send(const T& src, bool is_blocking);
    std::pair<bool, bool> chan_recv(T& dst, bool is_blocking);
public:
    explicit Chan(size_t n = 0);

    // Copy constructor
    Chan(const Chan &c);

    // Move constructor
    Chan(Chan &&c);

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
Chan<T>::Chan(size_t n) : buffer(n) {};

/*
template<typename T>
Chan<T>::Chan(const Chan &c) :
    buffer(c.buffer),
    send_queue(c.send_queue),
    recv_queue(c.recv_queue),
    is_closed(),
    chan_lock() {
        is_closed = c.is_closed.load();
    }
*/

template<typename T>
Chan<T>::Chan(Chan &&c) :
    buffer(std::move(c.buffer)),
    send_queue(std::move(c.send_queue)),
    recv_queue(std::move(c.recv_queue)),
    is_closed(),
    chan_lock() {
        is_closed = c.is_closed.exchange(0);
    }

template<typename T>
void Chan<T>::send(const T& src) {
    chan_send(src, true);
}

template<typename T>
T Chan<T>::recv() {
    T temp;
    recv(temp);
    return temp;
}

template<typename T>
bool Chan<T>::recv(T& dst) {
    std::pair<bool, bool> selected_received = chan_recv(dst, true);
    return selected_received.second;
}

template<typename T>
bool Chan<T>::send_nonblocking(const T& src) {
    return chan_send(src, false);
}

template<typename T>
bool Chan<T>::recv_nonblocking(T& dst) {
    std::pair<bool, bool> selected_received = chan_recv(dst, false);
    return selected_received.first;
}

template<typename T>
bool Chan<T>::chan_send(const T& src, bool is_blocking) {
    // Fast path: check for failed non-blocking operation without acquiring the lock.
    if (!is_blocking
        && !is_closed
        && ((buffer.capacity() == 0 && recv_queue.empty()) || (buffer.capacity() > 0 && buffer.is_full())))
        return false;

    // scoped_lock can't be used b/c we must .unlock() prior to future.get()
    std::unique_lock<std::mutex> lck{chan_lock};

    // sending to a closed channel is an error.
    if (is_closed) {
        throw "send on closed channel"; // TODO error type.
    }

    // if a waiting receiver exists,
    // pass the value we want to send directly to the receiver,
    // bypassing the buffer (if any).
    if (!recv_queue.empty()) {
        // Get the first promise pointer on the queue
        std::promise<T>& promise_ptr = recv_queue.front();
        recv_queue.pop();

        // give the promise the value of src
        promise_ptr.set_value(src);
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

    std::pair<std::promise<void>*, T> promise_data_pair(&promise, src);
    send_queue.push(promise_data_pair);

    lck.unlock();

    // if close() passes an exception, rethrow to user.
    // Note that there is no data passing here (ex. dst = future.get()), b/c done in chan_recv.
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
std::pair<bool, bool> Chan<T>::chan_recv(T& dst, bool is_blocking) {
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
        std::pair<std::promise<void>*, T>& promise_data_pair = send_queue.front(); // TODO beware mem
        send_queue.pop();

        if (buffer.capacity() == 0) {
            dst = promise_data_pair.second; // TODO beware mem
        } else {
            dst = buffer.front(); // TODO beware mem
            buffer.pop();
            buffer.push(promise_data_pair.second);
        }
        promise_data_pair.first->set_value(); // sender is unblocked.
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
    catch (...) { // TODO organize exceptions
        // ignore exception passed by close(), b/c !is_closed indicator is returned to user.
    }

    return std::pair<bool, bool>(true, !is_closed);
}

template<typename T>
void Chan<T>::foreach(std::function<void(T)> f){
    T cur_data;
    bool received = recv(cur_data);
    while (received) {
        f(cur_data);
        received = recv(cur_data);
    }
}

template<typename T>
void Chan<T>::close(){
    std::unique_lock<std::mutex> lck{chan_lock};

    if (is_closed) {
        // TODO organize exceptions.
        throw "cannot close a closed channel";
    }

    is_closed = true;

    // release all receivers.
    while (!recv_queue.empty()) {
        std::promise<T>& promise_ptr = recv_queue.front();
        recv_queue.pop();
        // instead of passing some data indicating close() to the future, pass exception for clarity.
        // the waiting receiver should handle this exception.
        // TODO organize exceptions.
        promise_ptr.set_exception(std::make_exception_ptr(std::exception()));
    }

    // release all senders.
    // By Go semantics, senders should throw exception to users.
    while (!send_queue.empty()) {
        std::pair<std::promise<void>*, T>& promise_data_pair = send_queue.front();
        send_queue.pop();
        // the waiting sender should rethrow this exception.
        // TODO organize exceptions.
        promise_data_pair.first->set_exception(std::make_exception_ptr(std::exception()));
    }
}

#endif
