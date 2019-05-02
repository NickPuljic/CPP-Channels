#include "../../../chan.h"
#include <iostream>
#include <chrono>

using namespace std;

int main() {
    int i = 50;

    auto start = std::chrono::high_resolution_clock::now();

    for (int n = 0; n < 500000; n++) {

        Chan<int> bufferedChannel(i);

        for (int m = 0; m < i; m++) {
            bufferedChannel.send(0);
        }

        bufferedChannel.close();

        bufferedChannel.foreach([=](int num) mutable {
            int _ = num;
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    cout << "Program took: " << elapsed.count() << "\n";

    return 0;
}
