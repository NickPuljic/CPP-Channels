#ifndef BUFFER_H
#define BUFFER_H

#include <queue>
#include <future>
#include <atomic>

// unlike channels in Go, we modularize buffer management
// TODO decide if all methods need to be atomic. If so, use a mutex.
// TODO decide if memory order need to be specified.
template<typename T>
class Buffer {
private:
    std::queue<T> q;
    size_t cap;
    // cur_size is atomic to enable lock-free fast-track condition in chan_recv
    // note: ++, --, operator= on cur_size are atomic
    std::atomic<size_t> cur_size{0};
public:
    explicit Buffer(size_t n = 0);
    // Copy constructor
    Buffer(const Buffer &b);
    // Move constructor
    Buffer(Buffer &&b);

    // Copy push()
    void push(const T& elem);
    // Move push()
    void push(T&& elem);
    T& front();
    void pop();

    size_t current_size();
    size_t capacity();
    bool is_full();
};

template<typename T>
Buffer<T>::Buffer(size_t n) {
    cap = n;
}

template<typename T>
Buffer<T>::Buffer(const Buffer &b) : q(b.q), cap(b.cap), cur_size() {
    cur_size = b.cur_size.load();
}

template<typename T>
Buffer<T>::Buffer(Buffer &&b) : q(std::move(b.q)), cap(std::move(b.cap)), cur_size() {
    cur_size = b.cur_size.exchange(0);
}

template<typename T>
void Buffer<T>::push(const T& elem) {
    q.push(elem);
    cur_size++;
}

template<typename T>
void Buffer<T>::push(T&& elem) {
    q.push(elem);
    cur_size++;
}

template<typename T>
T& Buffer<T>::front() {
    return q.front();
}

template<typename T>
void Buffer<T>::pop() {
    q.pop();
    cur_size--;
}

template<typename T>
size_t Buffer<T>::current_size() {
    return cur_size.load();
}

template<typename T>
size_t Buffer<T>::capacity() {
    return cap;
}

template<typename T>
bool Buffer<T>::is_full() {
    return cap == cur_size.load();
}

#endif
