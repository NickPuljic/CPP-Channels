#define CATCH_CONFIG_MAIN
#include "libs/catch.hpp"
#include "main.cpp"


void send_n_to_channel(Chan<int>& chan, int n) {
    for (int i = 0; i < n; i++) {
        chan.send(i);
    }
}

void recv_n_from_channel(Chan<int>& chan, int n) {
    int num;
    for (int i = 0; i < n; i++) {
        chan.recv(num);
        REQUIRE(num == i);
    }
}

void recv_n(Chan<int>& chan, int n) {
    int num;
    chan.recv(num);
    REQUIRE(num == n);
}

TEST_CASE( "sending and receiving" ) {
    Chan<int> chan = Chan<int>(15);

    SECTION( "sending and receiving async" ) {
        std::thread t1{send_n_to_channel, std::ref(chan), 15}; // Why std::ref? - Nick

        // Wait for them all to send
        t1.join();

        std::thread t2{recv_n_from_channel, std::ref(chan), 15};

        t2.join();
    }
    SECTION( "receiving first sync" ) {
        std::thread t1{recv_n_from_channel, std::ref(chan), 15};

        // Give the recv a second to make sure it is waiting
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::thread t2{send_n_to_channel, std::ref(chan), 15};

        t2.join();
        t1.join();
    }
    SECTION( "receiving twice first sync" ) {
        std::thread t1{recv_n, std::ref(chan), 0};
        // Give the rec a second to wait
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::thread t2{recv_n, std::ref(chan), 1};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::thread t3{send_n_to_channel, std::ref(chan), 2};

        t3.join();
        t1.join();
        t2.join();
    }
    SECTION( "sending first sync" ) {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};
        std::thread t2{recv_n_from_channel, std::ref(chan), 15};

        t1.join();
        t2.join();
    }
}
