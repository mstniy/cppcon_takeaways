#ifndef LAZY_FUTURE_H
#define LAZY_FUTURE_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <variant>
#include <exception>
#include <vector>

namespace lazy {

	namespace detail {
		template<typename P, typename Fun>
		struct then_promise_ {
			P p_;
			Fun fun_;
			template<typename... VS>
			void set_value(VS&&... vs) {
				try {
					p_.set_value(fun_(std::forward<VS>(vs)...));
				}
				catch(...) {
					p_.set_exception(std::current_exception());
				}
			}
			template<typename E>
			void set_exception(E e) {p_.set_exception(e);}
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

			template<int I, typename... XS> void set(XS&&... xs) {
				std::unique_lock<std::mutex> lk(pst->mtx);
				pst->data.template emplace<I>(std::forward<XS>(xs)...);
				pst->cv.notify_one();
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<2>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<1>(e);}
		};

		template<typename T>
		struct vector_multi_wait_all_state_ {
			std::mutex mtx;
			std::condition_variable cv;
			std::vector<std::variant<std::exception_ptr, T>> datas;
			std::size_t num_finished = 0;
			vector_multi_wait_all_state_(std::size_t size):
				datas(size)
			{}
		};

		template<typename MultiWaitState>
		struct vector_multi_wait_all_promise_ {
			MultiWaitState* pst;
			std::size_t index;

			template<int I, typename... XS> void set(XS&&... xs) {
				std::unique_lock<std::mutex> lk(pst->mtx);
				pst->datas[index].template emplace<I>(std::forward<XS>(xs)...);
				pst->num_finished += 1;
				if (pst->num_finished == pst->datas.size())
					pst->cv.notify_one();
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<1>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<0>(e);}

			vector_multi_wait_all_promise_(MultiWaitState* state, std::size_t index):
				pst(state),
				index(index)
			{}
		};

		template<typename... T>
		struct multi_wait_all_state_ {
			std::mutex mtx;
			std::condition_variable cv;
			std::tuple<std::variant<std::exception_ptr, T>...> datas;
			std::size_t num_finished = 0;
			static constexpr std::size_t num_tasks = sizeof...(T);
		};

		template<typename MultiWaitState, int I>
		struct multi_wait_all_promise_ {
			MultiWaitState* pst;

			template<int J, typename... XS> void set(XS&&... xs) {
				std::unique_lock<std::mutex> lk(pst->mtx);
				std::get<I>(pst->datas).template emplace<J>(std::forward<XS>(xs)...);
				pst->num_finished += 1;
				if (pst->num_finished == pst->num_tasks)
					pst->cv.notify_one();
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<1>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<0>(e);}
		};

		template<class Tasks, class MultiWaitState, std::size_t... Indices>
		auto wait_all_helper_(Tasks tasks, MultiWaitState* state, std::index_sequence<Indices...>)
		{
			(..., std::get<Indices>(tasks).cb(detail::multi_wait_all_promise_<MultiWaitState, Indices>{state}));

			if (true) {
				std::unique_lock<std::mutex> lk(state->mtx);
				state->cv.wait(lk, [state]() {
					return state->num_finished == state->num_tasks;
				});
			}

			return std::move(state->datas);
		}

		template<typename Result, typename Callback>
		struct typed_task_
		{
			using ResultType = Result;
			Callback cb;

			template<typename Fun>
			auto then(Fun fun) {
				auto lmbd = [cb=std::move(cb), fun=std::move(fun)](auto p) mutable{
					cb(detail::then_promise_<decltype(p), Fun>{std::move(p), std::move(fun)});
				};
				if constexpr (std::is_void_v<Result>){
					using FunResult = std::result_of_t<Fun&&()>;
					return detail::typed_task_<FunResult, decltype(lmbd)>{std::move(lmbd)};
				}
				else {
					using FunResult = std::result_of_t<Fun&&(Result&&)>;
					return detail::typed_task_<FunResult, decltype(lmbd)>{std::move(lmbd)};
				}
			}
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

	template<class Task>
	auto wait(Task task) {
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

	// Returns std::vector<std::variant<std::exception_ptr, Task::ResultType>>
	template<class Task>
	auto wait_all(std::vector<Task> tasks) {
		detail::vector_multi_wait_all_state_<typename Task::ResultType> state(tasks.size());
		for (std::size_t i=0; i<tasks.size(); i++)
			tasks[i].cb(detail::vector_multi_wait_all_promise_<decltype(state)>{&state, i});

		if (true) {
			std::unique_lock<std::mutex> lk(state.mtx);
			state.cv.wait(lk, [&tasks, &state]() {
				return state.num_finished == tasks.size();
			});
		}

		return std::move(state.datas);
	}

	// Returns std::tuple<std::variant<std::exception_ptr, Tasks::ResultType>...>
	template<class... Tasks>
	auto wait_all(Tasks... tasks) {
		detail::multi_wait_all_state_<typename Tasks::ResultType...> state;
		return detail::wait_all_helper_(std::forward_as_tuple(tasks...), &state, std::make_index_sequence<sizeof...(Tasks)>{});
	}

	// TODO: wait_all_windows to start at most N tasks at once.
	// TODO: async waits
};

#endif
