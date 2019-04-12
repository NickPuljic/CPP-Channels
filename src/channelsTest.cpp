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

void recv_assignment_n_from_channel(Chan<int>& chan, int n) {
    int num;
    for (int i = 0; i < n; i++) {
        num = chan.recv();
        REQUIRE(num == i);
    }
}

void send_n(Chan<int>& chan, int n) {
    chan.send(n);
}

void recv_n(Chan<int>& chan, int n) {
    int num;
    chan.recv(num);
    REQUIRE(num == n);
}

void must_stay_blocked(Chan<int>& chan) {
    chan.send(-1);
    // This line can never execute
    REQUIRE(1 == 2);
}

TEST_CASE( "unbuffered channel" ) {
    Chan<int> chan;

    SECTION( "unbuffered blocking two channels") {
        std::thread t1{send_n_to_channel, std::ref(chan), 1};
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::thread t2{must_stay_blocked, std::ref(chan)};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int i = chan.recv();
        REQUIRE(i == 0);
        t1.join();
        t2.detach();
    }
    SECTION( "unbuffered receive twice") {
        std::thread t1{send_n, std::ref(chan), 5};
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::thread t2{send_n, std::ref(chan), 7};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        REQUIRE(chan.recv() == 5);
        REQUIRE(chan.recv() == 7);

        t1.join();
        t2.join();
    }
}

TEST_CASE( "sending and receiving" ) {
    Chan<int> chan = Chan<int>(15);

    SECTION( "sending and receiving async" ) {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};

        // Wait for them all to send
        t1.join();

        std::thread t2{recv_n_from_channel, std::ref(chan), 15};

        t2.join();
    }
    SECTION( "recv assignment") {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};

        // Wait for them all to send
        t1.join();

        std::thread t2{recv_assignment_n_from_channel, std::ref(chan), 15};

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
