#include <thread>
#include <future>
#include <list>

#include "threading.hpp"

namespace filehasher {

struct thread_group::thread_group_impl {
    std::list<std::future<void>> tasks;

    thread_group_impl()
    {}

    void wait() {
        for(auto&& t : tasks) t.wait();
        tasks.clear();
    }

    void join() {
        for(auto&& t : tasks) t.wait();

        // Cleare the list of stored awaitables before "getting" them.
        // If "get" will raise exception - invalid futures will be stored.
        // All job is alredy done.
        decltype(tasks) tmp;
        tmp.swap(tasks);

        for(auto&& t : tmp) t.get();
        tasks.clear();
    }

    void do_launch(std::packaged_task<void()>&& task) {
        tasks.emplace_back(task.get_future());
        std::thread([t = std::move(task)] () mutable {
            t();
        }).detach();
    }
};

thread_group::thread_group() : pimp(std::make_unique<thread_group_impl>())
{}

thread_group::~thread_group() {
    pimp->wait();
}

void thread_group::join() {
    pimp->join();
}

void thread_group::wait() {
    pimp->wait();
}

void thread_group::do_launch(std::packaged_task<void()>&& task) {
    pimp->do_launch(std::move(task));
}

}// namespace filehasher