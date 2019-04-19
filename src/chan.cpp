#include "chan.h"

template<typename T>
void Chan<T>::iterator::next() {
    bool received = chan_.recv(cur_data_); // TODO: Nick - why doesnt this block?
    if (!received) {
        is_end_ = true;
    }
}

template<typename T>
Chan<T>::iterator::iterator(Chan& chan, bool is_end) : chan_(chan), is_end_(is_end) {
    if (!is_end_) {
        next();
    }
}

template<typename T>
T Chan<T>::iterator::operator*() const { return cur_data_; }

template<typename T>
iterator& Chan<T>::iterator::operator++() {
    next();
    return *this;
}

template<typename T>
bool Chan<T>::iterator::operator!=(const iterator& lhs, const iterator& rhs) {
    return !(lhs == rhs);
}

template<typename T>
iterator Chan<T>::iterator::begin() { return iterator(*this, false); }

template<typename T>
iterator Chan<T>::iterator::end()   { return iterator(*this, true); }

template<typename T>
bool Chan<T>::iterator::operator==(const iterator& lhs, const iterator& rhs) {
    if ((lhs.is_end_ == true) && (rhs.is_end_ == true)) {
        return true;
    } else {
        // because we only use iterator to implement for range loop,
        // we skip the non-end comparison for now.
        // this is bad, since we are still exposing the iterator.
        // TODO fix this.
        return false;
    }
}

template<typename T>
Chan<T>::Chan(const Chan &c) :
    buffer(c.buffer),
    send_queue(c.send_queue),
    recv_queue(c.recv_queue),
    is_closed(),
    chan_lock() {
        is_closed = c.is_closed.load();
    }

template<typename T>
Chan<T>::Chan(Chan &&c) :
    buffer(std::move(c.buffer)),
    send_queue(std::move(c.send_queue)),
    recv_queue(std::move(c.recv_queue)),
    is_closed(),
    chan_lock() {
        is_closed = c.is_closed.exchange(0);
    }

// TODO understand compiler error: "Explicitly initialize member which does not have a default constructor"
template<typename T>
Chan<T>::Chan(size_t n) : buffer(n) {};

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
    // TODO can "this" chan be null, or is this Go-specific? see if c == nil ... in chansend().

    // Fast path: check for failed non-blocking operation without acquiring the lock.
    // TODO read chan.go's notes on single-word read optimization here. can do same using Buffer class?
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
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    // TODO explain why pair; cannot memcpy handle.
    // TODO change pair to struct/class. beware of multiple copies here.
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
    std::unique_lock<std::mutex> lck{chan_lock};

    if (is_closed) {
        // TODO organize exceptions.
        throw "cannot close a closed channel";
    }

    is_closed = true;

    // release all receivers.
    // TODO WHY? by invariant?
    while (!recv_queue.empty()) {
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
