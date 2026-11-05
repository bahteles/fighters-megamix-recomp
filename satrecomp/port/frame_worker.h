// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
namespace port {
// One render/CPU producer. SDL window events, OpenGL and swap calls stay on the main thread.
class FrameWorker {
    std::mutex mutex;std::condition_variable wake;std::function<void()> task;bool stop=false;std::thread thread;
public:
    FrameWorker():thread([this]{for(;;){std::function<void()> run;{std::unique_lock lock(mutex);wake.wait(lock,[&]{return stop||bool(task);});if(stop)return;run=std::move(task);task={};}run();}}){}
    ~FrameWorker(){{std::lock_guard lock(mutex);stop=true;}wake.notify_all();thread.join();}
    template<class F> auto Run(F fn){
        using Result=decltype(fn());auto job=std::make_shared<std::packaged_task<Result()>>(std::move(fn));auto result=job->get_future();
        {std::lock_guard lock(mutex);task=[job]{(*job)();};}wake.notify_one();return result;
    }
};
}
