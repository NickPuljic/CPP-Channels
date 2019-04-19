#include "buffer.h"

explicit Buffer::Buffer(size_t n) {
    cap = n;
}

Buffer::Buffer(const Buffer &b) :
    q(b.q),
    cap(b.cap),
    cur_size() {
        cur_size = b.cur_size.load();
    }

Buffer::Buffer(Buffer &&b) :
    q(std::move(b.q)),
    cap(std::move(b.cap)),
    cur_size() {
        cur_size = b.cur_size.exchange(0);
    }

void Buffer::push(const T& elem) {q.push(elem); cur_size++;}
void Buffer::push(T&& elem) {q.push(elem); cur_size++;}

T& Buffer::front() {return q.front();}
void Buffer::pop() {q.pop(); cur_size--;}

size_t Buffer::current_size() {return cur_size.load();}
size_t Buffer::capacity() {return cap;}
bool Buffer::is_full() {return cap == cur_size.load();}
