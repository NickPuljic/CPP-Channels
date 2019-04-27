#include "../../../chan.h"
#include <iostream>
//#include <random>
#include <chrono>

using namespace std;

/*class Rand_int {
public:
  Rand_int(int high, int seed) :dist{0, high} { re.seed(seed); }
  int operator()() { return dist(re); }
private:
  default_random_engine re;
  uniform_int_distribution<> dist;
};*/


int main() {
    //Rand_int rnd {99, 1};
    int i = 20;

    auto start = std::chrono::high_resolution_clock::now();

    for (int n = 0; n < 500000; n++) {
        //int i = rnd() + 1;

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
