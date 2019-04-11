#include <iostream>
#include <queue>
#include <future>

// TODO Are following functions/components from chan.go necessaryy?
// send
// sendDirect
// recvDirect
// race controls
// debug


// unlike chan.go, we modularize buffer management.
// implemented using std::queue for MVP.
// TODO impl using circular buffer.
// TODO should Buffer be a nested class in Chan?
template<typename T>
class Buffer {
private:
    std::queue<T> q;
    unsigned cap;
    unsigned cur_size;

public:
    // TODO use explicit specifier?
    Buffer(unsigned n) {cap = n; cur_size = 0;}

    // TODO impl ~Buffer();
    // TODO are copy and move needed if private class?

    void push(const T& elem) {cur_size++; q.push(elem);} // copy elem
    void push(T&& elem) {cur_size++; q.push(elem);} // move elem
    T& front() {return q.front();}
    // TODO also need const_reference front() like std::queue?
    void pop() {cur_size--; q.pop();}

    // TODO use size_t?
    unsigned current_size() {return cur_size;}
    unsigned capacity() {return cap;}
    bool is_full() {return cur_size == cap;}
};

template<typename T> // TODO rename T?
class Chan {
private:
    Buffer<T> buffer;
    // queues for waiting senders and receivers, respectively.
    // using a std::promise object, we pass a value (TODO or an exception if channel is closed),
    // that is acquired asynchronously by a corresponding std::future object.
    // a blocking sender needs an address to send its data to, and a blocking receiver needs the data.
    // TODO explain why ptr used, due to std::queue impl.
    std::queue<std::pair<std::promise<void>*, T>> send_queue;
    std::queue<std::promise<T>*> recv_queue;
    bool is_closed;
    std::mutex chan_lock;

    bool chan_send(const T& src, bool is_blocking);
    // TODO comment this pair. see chan.go.
    std::pair<bool, bool> chan_recv(T& dst, bool is_blocking);

public:
    // TODO use explicit keyword?
    Chan(unsigned n);

    // TODO impl destructor, copy, and move.

    // TODO go channels return T/F. change void to T/F.
    void send(const T& src);
    // TODO dst as return value? how to return T/F?
    void recv(T& dst);

    // non-blocking versions of send and recv.
    // for now, expose the non-blocking versions to the user,
    // who can combine them in if/else block to simulate the select stmt,
    // until the select stmt is implemented.
    // TODO move to private once select stmt added.
    void send_nonblocking(const T& src);
    void recv_nonblocking(T& dst);

    void close();
};


// TODO understand compiler error: "Explicitly initialize member which does not have a default constructor"
template<typename T>
Chan<T>::Chan(unsigned n) : buffer(n) {};


template<typename T>
void Chan<T>::send(const T& src) {
    chan_send(src, true);
}


template<typename T>
void Chan<T>::recv(T& dst) {
    chan_recv(dst, true);
}


template<typename T>
bool Chan<T>::chan_send(const T& src, bool is_blocking) {
    // TODO can "this" chan be null, or is this Go-specific? see if c == nil ... in chansend().

    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // TODO read chan.go's notes on single-word read optimization here. can do same using Buffer class?
    if (!is_blocking
        && !is_closed
        && ((buffer.capacity() == 0 && recv_queue.empty()) || (buffer.capacity() > 0 && buffer.is_full())))
        return false;

    // scoped_lock can't be used b/c we must .unlock() prior to future.get().
    std::unique_lock lck{chan_lock};

    // sending to a closed channel is an error.
    if (is_closed) {
        throw "send on closed channel"; // TODO error type.
    }

    // if a waiting receiver exists,
    // pass the value we want to send directly to the receiver,
    // bypassing the buffer (if any).
    if (!recv_queue.empty()) {
        std::promise<T>* promise_ptr = recv_queue.front();
        recv_queue.pop();
        promise_ptr->set_value(src); // TODO comment what this is doing.
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
    // TODO explain why ptr (handle is on heap).
    std::promise<void>* promise_ptr = new std::promise<void>;
    std::future<void> future = promise_ptr->get_future();
    // TODO explain why pair; cannot memcpy handle.
    // TODO change pair to struct/class. beware of multiple copies here.
    std::pair<std::promise<void>*, T> promise_data_pair(promise_ptr, src);
    send_queue.push(promise_data_pair);
    lck.unlock();
    future.get();
    // TODO what if exception sent over future?
    // Note that there is no data passing here, b/c done in chan_recv.
    return true;
}


// receives on channel c and writes the received data to dst.
// if not blocking and no elements are available, returns (false, false).
// else if c is closed, zeros dst and returns (true, false). TODO why zero dst?
// else, fills in dst with an element and returns (true, true).
// A non-nil dst must refer to the heap or the caller's stack.
// two bools in a pair are (selected, received).
// TODO pair to struct/class.
// TODO dst may be nil, in which case received data is ignored.
template<typename T>
std::pair<bool, bool> Chan<T>::chan_recv(T& dst, bool is_blocking) {
    // TODO can "this" chan be null, or is this Go-specific? see chanrecv().

    std::unique_lock lck{chan_lock};

    // from chan.go:
    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // The order of operations is important here: reversing the operations can lead to
    // incorrect behavior when racing with a close.
    // TODO read the equivalent in chan.go. loads from buffer.capacity() and closed must be atomic.
    // locked at entrance for now.
    if (!is_blocking
        && (buffer.capacity() == 0 && send_queue.empty() || buffer.capacity() > 0 && buffer.current_size())
        && !is_closed) {
        return std::pair<bool, bool>(false, false);
    }

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
    std::promise<T>* promise_ptr;
    std::future<T> future = promise_ptr->get_future();
    recv_queue.push(promise_ptr);

    lck.unlock();
    dst = future.get();
    return std::pair<bool, bool>(true, !is_closed);
}

void f1(Chan<int>& chan) {
    chan.send(1);
    chan.send(2);
    chan.send(3);
    std::cerr << "t1 done" << std::endl;
}

void f2(Chan<int>& chan) {
    int num = 0;
    for (auto i = 0; i < 3; i++) {
        chan.recv(num);
        std::cout << "t2 received: " << num << std::endl;
    }
}

int main() {
    Chan<int> chan = Chan<int>(5);

    std::thread t1{f1, std::ref(chan)};

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::thread t2{f2, std::ref(chan)};

    t1.join();
    t2.join();

    return 0;
}