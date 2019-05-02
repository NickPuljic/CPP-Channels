#include "../../../chan.h"
#include <iostream>
#include <chrono>

using namespace std;

void send_to_channel(Chan<int> channel) {
    channel.send(0);
}

int main() {
    Chan<int> unbufferedChannel;

    auto start = std::chrono::high_resolution_clock::now();

    for (int n = 0; n < 500000; n++) {
        std::thread t1{send_to_channel, unbufferedChannel};

        unbufferedChannel.recv();

        t1.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    cout << "Program took: " << elapsed.count() << "\n";

    return 0;
}
