#include "worker.hpp"
#include <iostream>
#include <memory>
#include <vector>

int main() {
    auto samples = std::make_shared<std::vector<int>>();
    demo::Worker worker(16);
    for (int i = 0; i < 10; ++i) {
        if (worker.submit([samples, i] { samples->push_back(i * i); }) !=
            demo::Worker::Submission::accepted) return 1;
    }
    worker.close(); // No reads of consumer-owned data until the join completes.
    std::cout << "Processed " << samples->size() << " samples; failures: "
              << worker.failure_count() << '\n';
}
