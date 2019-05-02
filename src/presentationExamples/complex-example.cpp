#include <numeric>
#include <iostream>
#include "chan.h"

void send_task(Chan<int> chan, const std::vector<int>& each_sender_data) {
    for (auto num : each_sender_data)
        chan.send(num);
}

void recv_task(Chan<int> chan, std::vector<int>& each_recver_data) {
    chan.foreach(
        [&](int num){each_recver_data.push_back(num);}
    );
}

void parallel_send_and_recv(
    unsigned chan_size = 0,
    unsigned n_senders = 3,
    unsigned n_recvers = 3,
    unsigned send_upto = 1000) {

    Chan<int> chan(chan_size);

    // fill all_sender_data with [1,send_upto].
    std::vector<int> all_sender_data(send_upto);
    std::iota(all_sender_data.begin(), all_sender_data.end(), 1);

    // split all_sender_data (almost) equally among (except for last) senders.
    std::vector<std::vector<int>> each_sender_data;
    const std::size_t each_sender_data_size = all_sender_data.size() / n_senders;
    auto begin = all_sender_data.begin();
    for (auto i = 0; i < n_senders; ++i) {
        auto end = i == n_senders - 1 ? all_sender_data.end() : begin + each_sender_data_size;
        each_sender_data.push_back(std::vector<int>(begin, end));
        begin = end;
    }

    // vectors for receivers to fill.
    std::vector<std::vector<int>> each_recver_data(n_recvers, std::vector<int>(0));

    std::vector<std::thread> threads;

    for (auto i = 0; i < n_recvers; ++i) {
        std::thread thread{send_task, chan, std::ref(each_sender_data[i])};
        threads.push_back(std::move(thread));
    }

    // launch all senders.
    for (auto i = 0; i < n_senders; ++i){
        std::thread thread{recv_task, chan, std::ref(each_recver_data[i])};
        threads.push_back(std::move(thread));
    }

    // wait until all data is sent, and close the channel so that receivers' foreach can terminate.
    std::this_thread::sleep_for(std::chrono::seconds(3));
    chan.close();
    for (auto i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    // merge all receiver data.
    std::vector<int> all_recver_data;
    for (auto recver_data : each_recver_data) {
        std::cout << "received " << recver_data.size() << " ints" << std::endl;
        all_recver_data.insert(all_recver_data.end(), recver_data.begin(), recver_data.end());
    }

    std::sort(all_recver_data.begin(), all_recver_data.end());
    std::sort(all_sender_data.begin(), all_sender_data.end());

    assert(all_recver_data == all_sender_data);
}

int main() {
    parallel_send_and_recv();
}
