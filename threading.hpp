#ifndef FILEHASHER_THREADING_HPP
#define FILEHASHER_THREADING_HPP

#include <memory>
#include <future>
#include <queue>

namespace filehasher {

// Simple implementation of "thread group"
// Each job pushed to group converted to 'packaged_task<void>' and launched in separate thread.
// 'thread_group' stores futures for all runing jobs.
// Job's return value is ignored, but thread_group helps to propagate exceptions.
// 'join' will wait for complition of all stored jobs and propagate the first  raised exceptions.
// 'wait' will just wait for complition of all stored jobs.
//
// WARNING: "thread group" itlef is not thread-safe 
class thread_group {
    struct thread_group_impl;
    const std::unique_ptr<thread_group_impl> pimp;

public:
    thread_group();
    ~thread_group();

    template<class F>
    void launch(F&& task) {
        do_launch(std::packaged_task<void()>(std::forward<F>(task)));
    }

    void join();
    void wait();

private:
    void do_launch(std::packaged_task<void()>&& task);
};

template<class T>
class chanel {
    static_assert(std::is_move_constructible_v<T>);
    static_assert(std::is_move_assignable_v<T>);

    size_t                  capacity;
    std::atomic<bool>       closed;
    std::queue<T>           queue;
    std::mutex              mtx;
    std::condition_variable condition_push;
    std::condition_variable condition_pop;

public:
    explicit chanel(size_t capacity) : capacity(capacity), closed(capacity > 0 ? false : true)
    {}

    bool push(T&& invalue){
        if(closed) return false;

        {
            std::unique_lock<std::mutex> lock(mtx);
            if (queue.size() >= capacity) {
                condition_push.wait(lock, [this]{return ((queue.size() < capacity) || closed);});
                if(closed) return false;
            }
            queue.push(std::forward<T>(invalue));
        }
        
        condition_pop.notify_one();
        return true;
    }

    bool pop(T&& outvalue) {
        {
            std::unique_lock<std::mutex> lock{mtx};
            if(queue.empty()) {
                if(closed) return false;
                condition_pop.wait(lock, [this]{return (!queue.empty() || closed);});
                if(closed && queue.empty()) return false;
            }
            outvalue = std::move(queue.front());
            queue.pop();
        }

        condition_push.notify_one();
        return true;
    }

    void close() {
        bool wasclosed = closed.exchange(true);
        if(!wasclosed) {
            condition_pop.notify_all();
            condition_push.notify_all();
        }
    }

    bool is_closed() {
        return closed;
    }
};

struct nan_value {};

template <bool> struct nanness {};
using nan_tag = nanness<true>;
using not_nan_tag = nanness<false>;

template<class J, class R = nan_value>
struct piped_workers_pool {
    static_assert(std::is_move_constructible_v<R>);
    static_assert(std::is_move_constructible_v<J>);
    static_assert(std::is_default_constructible_v<J>);

    thread_group               group;
    std::shared_ptr<chanel<J>> input;
    std::shared_ptr<chanel<R>> output;

public:
    template<class W>
    piped_workers_pool(size_t nworkers, size_t nqueue, W&& worker)
        : input(std::make_shared<chanel<J>>(nqueue)), output(std::make_shared<chanel<R>>(nqueue))
    {
        static_assert(std::is_copy_constructible_v<W>);
        static_assert(std::is_same_v<std::invoke_result_t<W,J>, R>);
        run(nworkers, std::forward<W>(worker));
    }

    template<class Unused, class W>
    piped_workers_pool(size_t nworkers, size_t nqueue, piped_workers_pool<Unused, J>& source, W&& worker)
        : input(source.output), output(std::make_shared<chanel<R>>(nqueue))
    {
        static_assert(std::is_copy_constructible_v<W>);
        static_assert(std::is_invocable_v<W,J>);
        run(nworkers, std::forward<W>(worker));
    }

    ~piped_workers_pool() {
        input->close();
        output->close();
        group.wait();
    }

    std::shared_ptr<chanel<J>> get_input_chan() {return input;}
    std::shared_ptr<chanel<R>> get_output_chan() {return output;}

    void wait() {
        group.join();
        output->close();
    }

private:
    template<class W>
    void run(size_t nworkers, W worker) {
        for(int i = 0; i < nworkers; i++) {
            group.launch([worker, this] () mutable {
                try {
                    J job_to_do{};
                    while(input->pop(std::move(job_to_do))) {
                        if(!call_and_pipe(worker, std::move(job_to_do), *output, nanness<std::is_same_v<nan_value, R>>()))
                            break;
                    }
                } catch (...) {
                    input->close();
                    output->close();
                    throw;
                }
            });
        }
    }

    template <class F>
    bool call_and_pipe(F& func, J&& p, chanel<R>& out, nan_tag ) {
        func(std::move(p));
        return !out.is_closed();
    }

    template <class F>
    bool call_and_pipe(F& func, J&& p, chanel<R>& out, not_nan_tag ) {
        return out.push(func(std::move(p)));
    }
};

}//namespace filehasher

#endif//FILEHASHER_THREADING_HPP
