// SPDX-License-Identifier: GPL-3.0-only
#include "../satrecomp/port/frame_worker.h"
#include <cassert>
#include <stdexcept>
int main() {
    port::FrameWorker worker;
    const auto mainThread = std::this_thread::get_id();
    std::thread::id producer;
    int calls = 0;
    for (int i = 0; i < 100; ++i) {
        auto result = worker.Run([&] {
            if (calls == 0) producer = std::this_thread::get_id();
            assert(producer != mainThread && producer == std::this_thread::get_id());
            return ++calls;
        });
        assert(result.get() == i + 1);
    }
    auto failure = worker.Run([]() -> int { throw std::runtime_error("job failed"); });
    bool caught = false;
    try { (void)failure.get(); } catch (const std::runtime_error &) { caught = true; }
    assert(caught);
    auto last = worker.Run([&] { return ++calls; });
    assert(last.get() == 101);
}
