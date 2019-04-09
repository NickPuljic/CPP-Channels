#include <iostream>
#include <queue>
#include <future>

// TODO Are following functions/components needed?
// send
// sendDirect
// recvDirect
// race controls
// debug


// unlike chan.go, we modularize buffer management.
// TODO start w/ std::queue, then impl circular buffer.
// TODO nested class?
template<typename T>
class Buffer {
public:
    // TODO explicit keyword? see pg 80.
    Buffer(int);
    ~Buffer();

    // TODO copy and move needed if private class?

    void push(T&); // TODO ref?
    T& front(); // TODO ref?
    void pop();

    // TODO size_type?
    unsigned current_size();
    unsigned capacity();
    bool is_full();
};


template<typename T> // TODO rename T?
class Chan {
private:
    Buffer<T> buffer;
    // queues for blocking senders and receivers, respectively.
    // using a std::promise object, we pass a value or an exception,
    // that is acquired asynchronously by a corresponding std::future object.
    // a blocking sender needs an address to send its data to, and a blocking receiver needs the data.
    // TODO ptr or ref? does it matter?
    // TODO how many copies happening in promise/future?
    std::queue<std::pair<std::promise<void>, T>> send_queue; // TODO ref?
    std::queue<std::promise<T>> recv_queue;
    bool is_closed;
    std::mutex lock; // TODO better lock type?

    bool chan_send(T& from, bool is_blocking);
    std::pair<bool, bool> chan_recv(T& to, bool is_blocking);

public:
    // TODO explicit keyword? see pg 80.
    Chan(int);
    ~Chan();

    // TODO add copy and move

    // TODO struct
    void send(T&); // TODO ref?
    T& recv(); // TODO ref?

    // non-blocking versions of send and recv.
    // for now, expose the non-blocking versions to the user,
    // who can combine them in if/else block to simulate the select stmt,
    // until the select stmt is implemented.
    // TODO move to private once select stmt added.
    // TODO struct.
    void send_nb(T&); // TODO ref?
    T& recv_nb(); // TODO ref?

    void close();
};

template<typename T>
Chan<T>::Chan(int) {
    // TODO see makechan()
}

template<typename T>
Chan<T>::~Chan() {
    // TODO
}

template<typename T>
bool Chan<T>::chan_send(T& src, bool is_blocking) {
    // TODO can "this" chan be null, or is this Go-specific? see if c == nil ... in chansend().

    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // TODO read chan.go's notes on single-word read optimization here.
    if (!is_blocking
        && !is_closed
        && ((buffer.get_capacity() == 0 && recv_queue.empty()) || (buffer.capacity() > 0 && buffer.is_full())))
        return false;

    lock.lock(); // TODO any defer() in C++?

    // sending to a closed channel is an error.
    if (is_closed) {
        lock.unlock();
        throw "send on closed channel"; // TODO error type.
    }

    // if a waiting receiver exists,
    // pass the value we want to send directly to the receiver,
    // bypassing the buffer (if any).
    if (!recv_queue.empty()) {
        std::promise<T>& promise = recv_queue.front();
        recv_queue.pop();
        // TODO comment what this is doing.
        promise.set_value(src); // TODO ref, move, memory, etc
        lock.unlock();
        return true;
    }

    // if space is available in the buffer, enqueue the element to send.
    if (!buffer.is_full()) {
        buffer.push(src); // TODO ref, move, memory, etc
        lock.unlock();
        return true;
    }

    // if not blocking (select stmt), return false.
    // TODO why? explain why.
    if (!is_blocking) {
        lock.unlock();
        return false;
    }

    // block on the channel. Some receiver will complete our operation for us.
    // TODO explain this
    // TODO memory
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    // TODO explain this
    std::pair<std::promise<void>, T> promise_data_pair(promise, src); // TODO change pair to struct/class.
    send_queue.push(promise_data_pair);
    // Note that no data passing here, b/c done in chanrecv.
    // TODO what if exception sent over future?
    future.get();
    return true;
}

// receives on channel c and writes the received data to dst.
// TODO disallow heap?
// TODO dst may be nil, in which case received data is ignored.
// TODO pair to struct/class. two bools are (selected, received).
// if not blocking and no elements are available, returns (false, false).
// else if c is closed, zeros dst and returns (true, false). TODO why zero dst?
// else, fills in dst with an element and returns (true, true).
// A non-nil dst must refer to the heap or the caller's stack.
template<typename T>
std::pair<bool, bool> Chan<T>::chan_recv(T& dst, bool is_blocking) {
    // TODO can "this" chan be null, or is this Go-specific? see chanrecv().

    lock.lock();

    // from chan.go:
    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // The order of operations is important here: reversing the operations can lead to
    // incorrect behavior when racing with a close.
    // TODO read the equivalent in chan.go. loads from buffer.capacity() and closed must be atomic.
    // locked at entrance for now.
    if (!is_blocking
        && (buffer.capacity() == 0 && send_queue.empty() || buffer.capacity() > 0 && buffer.current_size())
        && !is_closed) {
        lock.unlock();
        return std::pair<bool, bool>(false, false);
    }

    // else if c is closed, returns (true, false).
    if (is_closed && buffer.current_size() == 0) {
        lock.unlock();
        return std::pair<bool, bool>(true, false);
    }

    // from chan.go:
    // Found a waiting sender. If buffer is size 0, receive value directly from sender.
    // Otherwise, receive from head of queue (buffer) and add sender's value to the tail of the queue
    // (both map to the same buffer slot because the queue (buffer) is full).
    // (i.e. if buffer was not full, no sender would be waiting)
    // TODO read the equivalent in chan.go. understand invariants. 2 cases b/c sender adds data.
    if (!send_queue.empty()) {
        std::pair<std::promise<void>, T>& promise_data_pair = send_queue.front(); // TODO mem
        send_queue.pop();
        if (buffer.capacity() == 0) {
            dst = promise_data_pair.second; // TODO mem
        } else {
            // TODO mem
            T data = buffer.front();
            buffer.pop();
            dst = data;
            buffer.push(promise_data_pair.second);
        }
        promise_data_pair.first.set_value(); // sender is unblocked.
        lock.unlock();
        return std::pair<bool, bool>(true, true);
    }

    // if buffer is not empty, recv from buffer.
    if (buffer.current_size() > 0) {
        // TODO repeated code.
        T data = buffer.front();
        buffer.pop();
        dst = data;
        lock.unlock();
        return std::pair<bool, bool>(true, true);
    }

    // if not blocking (select stmt), return false.
    if (!is_blocking) {
        lock.unlock();
        return std::pair<bool, bool>(false, true);
    }

    // block on the channel.
    

}




int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}