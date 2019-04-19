#include "buffer.h"

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
    std::pair<bool, bool> chan_recv(T& dst, bool is_blocking);
public:
    explicit Chan(size_t n = 0);

    // Copy constructor
    Chan(const Chan &c);

    // Move constructor
    Chan(Chan &&c);

    // TODO impl destructor?

    // blocking send (ex. chan <- 1) does not return a boolean.
    void send(const T& src);

    // return value indicates whether the communication succeeded.
    // the value is true if the value received was delivered by a successful send operation to the channel,
    // or false if it is a zero value generated because the channel is closed and empty.
    bool recv(T& dst);
    // Assignment recv
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

    // a custom iterator to enable the for range loop.
    // importantly, note that begin() and operator++ modifies the channel by calling its recv().
    // while we are implementing an iterator for the for range loop only,
    // this iterator needs to be much more robust, since we are exposing it to user.
    // TODO any way to hide to user and only use in for range loop?
    // TODO what is const_iterator? is it useful for us?
    // TODO move this to a different class/file.
    class iterator {
    private:
        Chan& chan_;  // to call chan.recv().
        bool is_end_; // indicates whether the iterator has reached the end.

        // cur_data keeps (a copy of) the data that was recv()ed.
        // cur_data cannot be T&, because at the end (one passed the last), it cannot reference any data.
        // TODO use pointer and void comparison to eliminate copies.
        //      But, to do so, we need to construct an empty T, to be passed to recv().
        T cur_data_;

        // used in both constructor and operator++.
        // TODO: Throw an error if you try to iterator over an un closed channel
        void next();

    public:
        // because std::iterator is deprecated in C++17, need to add the following 5 typedefs.
        // TODO understand how these typedefs are used.
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = void; // TODO std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        // required methods of iterator.
        // constructor is called by begin() and end() only.
        iterator(Chan& chan, bool is_end);

        T operator*() const; // TODO beware copy.

        // TODO is operator-> needed?

        // preincrement
        iterator& operator++();

        // TODO postincrement is more tricky.

        friend bool operator==(const iterator& lhs, const iterator& rhs);

        friend bool operator!=(const iterator& lhs, const iterator& rhs);
    };

    iterator begin();
    iterator end();
};
