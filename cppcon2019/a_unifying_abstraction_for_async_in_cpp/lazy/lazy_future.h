#ifndef LAZY_FUTURE_H
#define LAZY_FUTURE_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <variant>
#include <exception>

namespace lazy {

	namespace detail {
		template<typename P, typename Fun>
		struct then_promise_ {
			P p_;
			Fun fun_;
			void set_value(auto&&... vs) {p_.set_value(fun_(std::forward<decltype(vs)>(vs)...));}
			void set_exception(auto e) {p_.set_exception(e);}
		};

		template<typename T>
		struct wait_state_ {
			std::mutex mtx;
			std::condition_variable cv;
			std::variant<std::monostate, std::exception_ptr, T> data;
		};

		template<typename T>
		struct wait_promise_ {
			wait_state_<T>* pst;

			template<int I> void set(auto&&... xs) {
				std::unique_lock<std::mutex> lk(pst->mtx);
				pst->data.template emplace<I>(std::forward<decltype(xs)>(xs)...);
				pst->cv.notify_one();
			}

			void set_value(auto&&... vs) {set<2>(std::forward<decltype(vs)>(vs)...);}
			void set_exception(auto e) {set<1>(e);}
		};

		template<typename Result, typename Callback>
		struct typed_task_
		{
			using ResultType = Result;
			Callback cb;
		};
	};

	auto new_thread() {
		auto lmbd =  [](auto p){
			std::thread t{ [p = std::move(p)]() mutable {
				p.set_value();
			}};
			t.detach();
		};
		return detail::typed_task_<void, decltype(lmbd)>{std::move(lmbd)};
	}

	auto then(auto task, auto fun) {
		auto lmbd = [=](auto p){
			task.cb(detail::then_promise_<decltype(p), decltype(fun)>{p, fun});
		};
		using Fun = decltype(fun);
		using TaskResult = typename decltype(task)::ResultType;
		if constexpr (std::is_void_v<TaskResult>){
			using FunResult = std::result_of_t<Fun&&()>;
			return detail::typed_task_<FunResult, decltype(lmbd)>{std::move(lmbd)};
		}
		else {
			using FunResult = std::result_of_t<Fun&&(TaskResult&&)>;
			return detail::typed_task_<FunResult, decltype(lmbd)>{std::move(lmbd)};
		}
	}

	template<class Task>
	auto sync_wait(Task task) {
		using T = typename Task::ResultType;
		detail::wait_state_<T> state;
		task.cb(detail::wait_promise_<T>{&state});

		if (true) {
			std::unique_lock<std::mutex> lk(state.mtx);
			state.cv.wait(lk, [&state]{
				return state.data.index() != 0;
			});
		}

		if (state.data.index() == 1)
			std::rethrow_exception(std::get<1>(state.data));

		return std::move(std::get<2>(state.data));
	}
};

#endif
