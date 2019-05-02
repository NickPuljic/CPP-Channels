#include "../chan.h"
#include <iostream>

using namespace std;

void thread1(Chan<string> c) { // thread 1
    string data_1 = c.recv(); // receive
    cout << "Data from thread 1: " << data_1 << endl;
    c.send("thread 1 data"); // send
}

void thread2(Chan<string> c) {
    c.send("thread 2 data"); // send
    string data_2; c.recv(data_2);
    cout << "Data from thread 2: " << data_2 << endl; // receive
}

int main() {
    Chan<string> c; // unbuffered string channel

    thread t1{thread1, c};
    thread t2{thread2, c};

    t1.join(); t2.join();
}
