#include "buffer.h"

template<typename T>
Buffer<T>::Buffer(size_t n) {
    cap = n;
}

template<typename T>
Buffer<T>::Buffer(const Buffer &b) :
    q(b.q),
    cap(b.cap),
    cur_size() {
        cur_size = b.cur_size.load();
    }

template<typename T>
Buffer<T>::Buffer(Buffer &&b) :
    q(std::move(b.q)),
    cap(std::move(b.cap)),
    cur_size() {
        cur_size = b.cur_size.exchange(0);
    }

template<typename T>
void Buffer<T>::push(const T& elem) {q.push(elem); cur_size++;}
template<typename T>
void Buffer<T>::push(T&& elem) {q.push(elem); cur_size++;}

template<typename T>
T& Buffer<T>::front() {return q.front();}
template<typename T>
void Buffer<T>::pop() {q.pop(); cur_size--;}

template<typename T>
size_t Buffer<T>::current_size() {return cur_size.load();}
template<typename T>
size_t Buffer<T>::capacity() {return cap;}
template<typename T>
bool Buffer<T>::is_full() {return cap == cur_size.load();}
