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
    explicit Buffer(size_t n);

    // Copy constructor
    Buffer(const Buffer &b);

    // Move constructor
    Buffer(Buffer &&b);

    // TODO impl ~Buffer() if destructor needed

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
