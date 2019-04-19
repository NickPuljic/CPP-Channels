#include <queue>
#include <future>
#include <atomic>

// unlike chan.go, we modularize buffer management.
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
    explicit Buffer(size_t n) {cap = n;}

    // Copy constructor
    Buffer(const Buffer &b) :
        q(b.q),
        cap(b.cap),
        cur_size() {
            cur_size = b.cur_size.load();
        }

    // Move constructor
    Buffer(Buffer &&b) :
        q(std::move(b.q)),
        cap(std::move(b.cap)),
        cur_size() {
            cur_size = b.cur_size.exchange(0);
        }

    // TODO impl ~Buffer() if destructor needed

    // Copy push()
    void push(const T& elem) {q.push(elem); cur_size++;}
    // Move push()
    void push(T&& elem) {q.push(elem); cur_size++;}

    T& front() {return q.front();}
    void pop() {q.pop(); cur_size--;}

    size_t current_size() {return cur_size.load();}
    size_t capacity() {return cap;}
    bool is_full() {return cap == cur_size.load();}
};
