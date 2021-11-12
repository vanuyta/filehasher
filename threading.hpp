#ifndef FILEHASHER_THREADING_HPP
#define FILEHASHER_THREADING_HPP

#include <memory>
#include <future>
#include <boost/fiber/buffered_channel.hpp>

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
    std::unique_ptr<thread_group_impl> pimp;

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

inline size_t const max_queue = 256;

template<class T>
using chanel = boost::fibers::buffered_channel<T>;

struct nan_value {};
template <bool> struct nanness {};
using nan_tag = nanness<true>;
using not_nan_tag = nanness<false>;

template<class J, class R = nan_value>
struct piped_workers_pool {
    static_assert(std::is_move_constructible_v<R>);
    static_assert(std::is_move_constructible_v<J>);

    thread_group                group;
    std::shared_ptr<chanel<J>>  input;
    std::shared_ptr<chanel<R>>  output;

public:
    template<class W>
    piped_workers_pool(size_t count, W&& worker)
        : input(std::make_shared<chanel<J>>(max_queue)), output(std::make_shared<chanel<R>>(max_queue))
    {
        static_assert(std::is_copy_constructible_v<W>);
        run(count, std::forward<W>(worker));
    }

    template<class Unused, class W>
    piped_workers_pool(size_t count, piped_workers_pool<Unused, J>& source, W&& worker)
        : input(source.output), output(std::make_shared<chanel<R>>(max_queue))
    {
        static_assert(std::is_copy_constructible_v<W>);
        run(count, std::forward<W>(worker));
    }

    ~piped_workers_pool() {
        input->close();
        output->close();
        group.wait();
    }

    chanel<J>& get_input_chan() {return *input;}
    chanel<R>& get_output_chan() {return *output;}

    void wait() {
        group.join();
        output->close();
    }

private:
    template<class W>
    void run(size_t count, W worker) {
        for(int i = 0; i < count; i++) {
            group.launch([worker, this] {
                try {
                    for(J& j : *input)
                        if(!call_and_pipe(worker, std::move(j), *output, nanness<std::is_same_v<nan_value, R>>()))
                            break;
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
        return (out.push(func(std::move(p))) == boost::fibers::channel_op_status::success);
    }
};

}; //namespace filehasher

#endif//FILEHASHER_THREADING_HPP
