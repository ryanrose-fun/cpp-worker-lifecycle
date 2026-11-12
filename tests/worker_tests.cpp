#include "worker.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <vector>

void check(bool condition) {
    if (!condition) throw std::runtime_error("check failed");
}
int main() {
    using W = demo::Worker;
    try {
        bool rejected = false;
        try { W invalid(0); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected);
        std::vector<int> sequence;
        {
            W worker(32);
            for (int i = 0; i < 20; ++i)
                check(worker.submit([&sequence, i] { sequence.push_back(i); }) == W::Submission::accepted);
        } // destructor drains and joins
        check(sequence.size() == 20);
        for (int i = 0; i < 20; ++i) check(sequence[static_cast<std::size_t>(i)] == i);

        W bounded(1);
        std::promise<void> started, release;
        auto released = release.get_future().share();
        check(bounded.submit([&started, released] { started.set_value(); released.wait(); }) == W::Submission::accepted);
        started.get_future().wait();
        const auto second = bounded.submit([] {});
        const auto third = bounded.submit([] {});
        release.set_value(); // Always release before assertions can unwind into join.
        check(second == W::Submission::accepted);
        check(third == W::Submission::full);
        bounded.close();
        bounded.close();
        check(bounded.submit([] {}) == W::Submission::closed);

        W resilient(1024);
        std::atomic<int> completed{0};
        check(resilient.submit([] { throw std::runtime_error("synthetic failure"); }) == W::Submission::accepted);
        std::vector<std::jthread> producers;
        for (int p = 0; p < 4; ++p)
            producers.emplace_back([&] {
                for (int i = 0; i < 100; ++i)
                    check(resilient.submit([&] { ++completed; }) == W::Submission::accepted);
            });
        producers.clear();
        resilient.close();
        check(completed == 400);
        check(resilient.failure_count() == 1);
        std::cout << "PASS: validation, FIFO, destructor drain, backpressure, close, concurrent producers, exception isolation\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
