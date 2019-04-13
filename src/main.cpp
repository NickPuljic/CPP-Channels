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
// implemented using std::queue
// TODO decide if all methods need to be atomic. If so, use a mutex.
// TODO decide if memory order need to be specified.
template<typename T>
class Buffer {
private:
    std::queue<T> q;
    size_t cap;
    // current size is atomic to enable lock-free fast-track condition in chan_recv.
    // note that ++, --, operator= on cur_size are atomic.
    std::atomic<size_t> cur_size{0};

public:
    explicit Buffer(unsigned n) {cap = n;}

    // TODO impl ~Buffer();
    // TODO are copy and move needed if private class?

    void push(const T& elem) {q.push(elem); cur_size++;} // copy elem
    void push(T&& elem) {q.push(elem); cur_size++;} // move elem
    T& front() {return q.front();}
    void pop() {q.pop(); cur_size--;}

    size_t current_size() {return cur_size;}
    size_t capacity() {return cap;}
    bool is_full() {return cap == cur_size;}
};

template<typename T>
class Chan {
private:
    Buffer<T> buffer;
    // queues for waiting senders and receivers, respectively.
    // using a std::promise object, we pass a value (TODO or an exception if channel is closed),
    // that is acquired asynchronously by a corresponding std::future object.
    // a blocking sender needs an address to send its data to, and a blocking receiver needs the data.
    std::queue<std::pair<std::promise<void>*, T>> send_queue;
    std::queue<std::promise<T>*> recv_queue;

    // is_closed is atomic to enable lock-free fast-track condition in chan_recv.
    // note that assignment and operator= on cur_size are atomic.
    std::atomic<bool> is_closed{false};
    std::mutex chan_lock;

    bool chan_send(const T& src, bool is_blocking);
    // TODO comment this pair. see chan.go.
    std::pair<bool, bool> chan_recv(T& dst, bool is_blocking);

public:
    explicit Chan(unsigned n);
    Chan();

    // TODO impl destructor, copy, and move.

    // blocking send (ex. chan <- 1) does not return a boolean.
    void send(const T& src);
    
    // return value indicates whether the communication succeeded.
    // the value is true if the value received was delivered by a successful send operation to the channel,
    // or false if it is a zero value generated because the channel is closed and empty.
    bool recv(T& dst);

    T recv();
    
    // non-blocking versions of send and recv.
    // for now, expose the non-blocking versions to the user,
    // who can combine them in if/else block to simulate the select stmt,
    // until the select stmt is implemented.
    // TODO comment return value semantics.
    // TODO move to private once select stmt added.
    bool send_nonblocking(const T& src);
    bool recv_nonblocking(T& dst);

    void close();
};


// TODO understand compiler error: "Explicitly initialize member which does not have a default constructor"
template<typename T>
Chan<T>::Chan(unsigned n) : buffer(n) {};

template<typename T> // TODO Add test cases
Chan<T>::Chan() : buffer(0) {};


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
bool send_nonblocking(const T& src) {
    return chan_send(src, false);
}

template<typename T>
bool recv_nonblocking(T& dst) {
    std::pair<bool, bool> selected_received = chan_recv(dst, false);
    return selected_received.first;
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

    // scoped_lock can't be used b/c we must .unlock() prior to future.get()
    std::unique_lock lck{chan_lock};

    // sending to a closed channel is an error.
    if (is_closed) {
        throw "send on closed channel"; // TODO error type.
    }

    // if a waiting receiver exists,
    // pass the value we want to send directly to the receiver,
    // bypassing the buffer (if any).
    if (!recv_queue.empty()) {
        // Get the first promise pointer on the queue
        std::promise<T>* promise_ptr = recv_queue.front();
        recv_queue.pop();

        // give the promise the value of src
        promise_ptr->set_value(src);
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

    // if close() passes an exception, rethrow to user.
    // Note that there is no data passing here (ex. dst = future.get()), b/c done in chan_recv.
    future.get();

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

    std::unique_lock lck{chan_lock};

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
    recv_queue.push(&promise);

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
void Chan<T>::close(){
    std::unique_lock lck{chan_lock};

    if (is_closed) {
        // TODO organize exceptions.
        throw "cannot close a closed channel";
    }

    is_closed = true;

    // release all receivers.
    // TODO WHY? by invariant?
    while (!send_queue.empty()) {
        std::promise<T>* promise_ptr = recv_queue.front();
        recv_queue.pop();
        // instead of passing some data indicating close() to the future, pass exception for clarity.
        // the waiting receiver should handle this exception.
        // TODO consider passing a zero-ed T.
        // TODO organize exceptions.
        promise_ptr->set_exception(std::make_exception_ptr(std::exception()));
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
