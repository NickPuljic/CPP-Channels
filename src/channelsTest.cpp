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

TEST_CASE("nonblocking send and recive test") {
    Chan<int> c1;

    SECTION("test nonblocking send") {
        // Send should fail
        REQUIRE(c1.send_nonblocking(5) == false);

        // Recv 10 and block then wait a second
        std::thread t1{recv_n, std::ref(c1), 10};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Send 10 nonblocking and make sure it succeeds
        REQUIRE(c1.send_nonblocking(10) == true);
        t1.join();
    }
    SECTION("test nonblocking recv") {
        int r = 0;
        // Make sure you can't recv nonblocking
        REQUIRE(c1.recv_nonblocking(r) == false);

        // Send 10 then wait
        std::thread t1{send_n, std::ref(c1), 10};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Make sure you can now recv and get correct value
        REQUIRE(c1.recv_nonblocking(r) == true);
        REQUIRE(r == 10);
        t1.join();
    }
}

TEST_CASE("copy and move constructors") {
    Chan<int> c1(5);

    SECTION("test move constructor") {
        // Send c1 int 5 and 7
        std::thread t1{send_n, std::ref(c1), 5};
        t1.join();
        std::thread t2{send_n, std::ref(c1), 7};
        t2.join();

        // Make a move of c1 to c2
        Chan<int> c2 = std::move(c1);

        // Check that c2 now has 5 and 7
        std::thread t3{recv_n, std::ref(c2), 5};
        t3.join();
        std::thread t4{recv_n, std::ref(c2), 7};
        t4.join();

        // Check if 5 is still in c1 (it shouldnt be)
        std::thread t5{recv_n, std::ref(c1), -1};
        // Give t5 1 second to execute then detatch
        std::this_thread::sleep_for(std::chrono::seconds(1));
        t5.detach();
    }
    SECTION("test copy constructor") {
        // Send c1 int 5 and 7
        std::thread t1{send_n, std::ref(c1), 5};
        t1.join();
        std::thread t2{send_n, std::ref(c1), 7};
        t2.join();

        // Make a copy of c1 to c2
        Chan<int> c2 = c1;

        // Check that c2 now has 5 and 7
        std::thread t3{recv_n, std::ref(c2), 5};
        t3.join();
        std::thread t4{recv_n, std::ref(c2), 7};
        t4.join();

        // Check that c1 also has 5 and 7
        std::thread t5{recv_n, std::ref(c1), 5};
        t5.join();
        std::thread t6{recv_n, std::ref(c1), 7};
        t6.join();
    }
}

TEST_CASE("unbuffered channel") {
    Chan<int> chan;

    SECTION("unbuffered blocking two channels") {
        // Send value to unbuffered channel and block
        std::thread t1{send_n_to_channel, std::ref(chan), 1};
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // Should fail if unbuffered channel recv a second time
        std::thread t2{must_stay_blocked, std::ref(chan)};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Recv from first block and make sure its correct
        int i = chan.recv();
        REQUIRE(i == 0);
        t1.join();
        t2.detach();
    }
    SECTION("unbuffered receive twice") {
        // Send on two channels
        std::thread t1{send_n, std::ref(chan), 5};
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::thread t2{send_n, std::ref(chan), 7};
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Unblock the sends and verify correct
        REQUIRE(chan.recv() == 5);
        REQUIRE(chan.recv() == 7);

        t1.join();
        t2.join();
    }
}

TEST_CASE("sending and receiving") {
    Chan<int> chan = Chan<int>(15);

    SECTION("sending and receiving async") {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};

        // Wait for them all to send
        t1.join();

        std::thread t2{recv_n_from_channel, std::ref(chan), 15};

        t2.join();
    }
    SECTION("recv assignment") {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};

        // Wait for them all to send
        t1.join();

        std::thread t2{recv_assignment_n_from_channel, std::ref(chan), 15};

        t2.join();
    }
    SECTION("receiving first sync") {
        std::thread t1{recv_n_from_channel, std::ref(chan), 15};

        // Give the recv a second to make sure it is waiting
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::thread t2{send_n_to_channel, std::ref(chan), 15};

        t2.join();
        t1.join();
    }
    SECTION("receiving twice first sync") {
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
    SECTION("sending first sync") {
        std::thread t1{send_n_to_channel, std::ref(chan), 15};
        std::thread t2{recv_n_from_channel, std::ref(chan), 15};

        t1.join();
        t2.join();
    }
}

void send_n_and_close(Chan<int>& chan, int n) {
    send_n_to_channel(chan, n);
    chan.close();
}

void recv_n_using_for_range(Chan<int>& chan) {
    int i = 0;
    for (auto num : chan) {
        REQUIRE(num == i);
        i++;
    }
}

TEST_CASE( "send, close, and recv using for range" ) {
    Chan<int> chan = Chan<int>(3);

    SECTION("send and close then range") {
        // Send 0, 1, 2 to chan and close
        std::thread t1{send_n_and_close, std::ref(chan), 3};
        t1.join();

        // Range through channel and make sure it can recv
        std::thread t2{recv_n_using_for_range, std::ref(chan)};
        t2.join();
    }
    SECTION("for loop should have nothing") {
        chan.close();
        std::thread t1{recv_n_using_for_range, std::ref(chan)};
        t1.join();
    }
}

void send_all(Chan<int>& chan, std::vector<int>& v) {
    for (auto num : v) {
        chan.send(num);
    }
}

void recv_for_seconds(Chan<int>& chan, std::vector<int>& recver_data, unsigned& seconds) {

    auto start = std::chrono::high_resolution_clock::now();

    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < seconds) {
        int num;
        chan.recv(num); // assumes no close.
        recver_data.push_back(num);
    }
}

void send_and_recv(
    unsigned chan_size = 0,
    unsigned n_senders = 3,
    unsigned n_recvers = 3,
    unsigned send_upto = 1000,
    unsigned recv_for  = 5) {

    REQUIRE((n_senders > 0 && n_recvers > 0 && send_upto > 0 && recv_for > 0));

    Chan<int> chan(chan_size);

    // all_sender_vec contains each number in [1,send_upto], uniquely.
    std::vector<int> all_sender_data(send_upto);
    std::iota(all_sender_data.begin(), all_sender_data.end(), 1);

    // TODO needed?
    // shuffle the data.
    // std::random_shuffle(all_sender_data.begin(), all_sender_data.end());

    // split it equally among (except for last) senders.
    std::vector<std::vector<int>> each_sender_data;
    const std::size_t each_sender_data_size = all_sender_data.size() / n_senders;
    auto begin = all_sender_data.begin();
    for (auto i = 0; i < n_senders; ++i) {
        auto end = i == n_senders - 1 ? all_sender_data.end() : begin + each_sender_data_size;
        each_sender_data.push_back(std::vector<int>(begin, end));
        begin = end;
    }

    // n_recvers vectors for recvers to fill.
    std::vector<std::vector<int>> each_recver_data(
        n_recvers,
        std::vector<int>(0));

    // launch all recvers first.
    // each recveiver will recv everything it can for recv_for seconds.
    // !!! assumes all sent data will be recved during the given seconds.
    for (auto i = 0; i < n_recvers; ++i) {
        std::thread t{recv_for_seconds, std::ref(chan), std::ref(each_recver_data[i]), std::ref(recv_for)};
        t.detach();
    }

    // launch all senders.
    for (auto i = 0; i < n_senders; ++i){
        std::thread t{send_all, std::ref(chan), std::ref(each_sender_data[i])};
        t.detach();
    }

    // !!! assumes this thread sleep until all recvers and senders finish.
    std::this_thread::sleep_for(std::chrono::seconds(recv_for + 5));

    // merge each_recver_data
    std::vector<int> all_recver_data;
    for (auto recver_data : each_recver_data) {
        // DEBUG
        // std::cout << recver_data.size() << std::endl;
        all_recver_data.insert(all_recver_data.end(), recver_data.begin(), recver_data.end());
    }

    // sort (in case sender data was shuffled above)
    std::sort(all_recver_data.begin(), all_recver_data.end());
    std::sort(all_sender_data.begin(), all_sender_data.end());

    REQUIRE(all_recver_data == all_sender_data);
}

TEST_CASE( "parallel send and recv" ) {
    send_and_recv();
}
