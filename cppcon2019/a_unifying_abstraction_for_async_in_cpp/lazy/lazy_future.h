#ifndef LAZY_FUTURE_H
#define LAZY_FUTURE_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <variant>
#include <exception>
#include <vector>
#include <atomic>

namespace lazy {

	namespace detail {
		// void breaks generic code. We cannot have a void as a member of std::variant, for example.
		// So replace void with std::monostate
		template<typename T>
		struct void_fallback { typedef T type; };

		template<>
		struct void_fallback<void> { typedef std::monostate type; };

		template<typename T>
		using void_fallback_t = typename void_fallback<T>::type;

		template<typename P, typename Fun>
		struct then_promise_ {
			P p_;
			Fun fun_;
			template<typename... VS>
			void set_value(VS&&... vs) {
				try {
					using FunResult = std::result_of_t<Fun&&(VS&&...)>;
					if constexpr (std::is_void_v<FunResult>)
					{
						fun_(std::forward<VS>(vs)...);
						p_.set_value();
					}
					else
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
		struct vector_wait_all_state_ {
			std::mutex mtx;
			std::condition_variable cv;
			std::vector<std::variant<std::exception_ptr, T>> datas;
			std::size_t num_finished = 0;
			vector_wait_all_state_(std::size_t size):
				datas(size)
			{}
		};

		template<typename MultiWaitState>
		struct vector_wait_all_promise_ {
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

			vector_wait_all_promise_(MultiWaitState* state, std::size_t index):
				pst(state),
				index(index)
			{}
		};

		template<typename... T>
		struct wait_all_state_ {
			std::mutex mtx;
			std::condition_variable cv;
			std::tuple<std::variant<std::exception_ptr, T>...> datas;
			std::size_t num_finished = 0;
			static constexpr std::size_t num_tasks = sizeof...(T);
		};

		template<typename MultiWaitState, int I>
		struct wait_all_promise_ {
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
			(..., std::get<Indices>(tasks).cb(detail::wait_all_promise_<MultiWaitState, Indices>{state}));

			if (true) {
				std::unique_lock<std::mutex> lk(state->mtx);
				state->cv.wait(lk, [state]() {
					return state->num_finished == state->num_tasks;
				});
			}

			return std::move(state->datas);
		}

		template<typename P, typename... T>
		struct when_all_state_ {
			P p; // The promise to be called when all tasks finish
			std::tuple<std::variant<std::exception_ptr, T>...> datas;
			std::atomic<std::size_t> num_finished = 0;
			static constexpr std::size_t num_tasks = sizeof...(T);

			when_all_state_(P p):
				p(std::move(p))
			{}
		};

		template<typename MultiWaitState, int I>
		struct when_all_promise_ {
			MultiWaitState* pst;

			when_all_promise_(MultiWaitState* pst):pst(pst){}

			when_all_promise_(const when_all_promise_&) = delete;
			when_all_promise_(when_all_promise_&& o)
				: pst(o.pst) {
				o.pst = nullptr;
			}
			~when_all_promise_() {
				if (pst != nullptr)
				{
					std::size_t finish_id = pst->num_finished.fetch_add(1)+1;
					if (finish_id == pst->num_tasks) { // Last task to end calls the promise and deletes the state
						pst->p.set_value(std::move(pst->datas));
						delete pst;
					}
				}
			}

			template<int J, typename... XS> void set(XS&&... xs) {
				std::get<I>(pst->datas).template emplace<J>(std::forward<XS>(xs)...); // This is NOT a race. Different tasks (on different threads) write to different memory locations.
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<1>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<0>(e);}
		};

		template<class Tasks, class MultiWaitState, std::size_t... Indices>
		void when_all_helper_(Tasks tasks, MultiWaitState* state, std::index_sequence<Indices...>)
		{
			(..., std::get<Indices>(tasks).cb(detail::when_all_promise_<MultiWaitState, Indices>{state}));
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
			std::thread t;
			try {
				t = std::thread{ [p = std::move(p)]() mutable {
					p.set_value();
				}};
			}
			catch (...) {
				p.set_exception(std::current_exception());
			}
			t.detach();
		};
		return detail::typed_task_<void, decltype(lmbd)>{std::move(lmbd)};
	}

	template<class Task>
	auto wait(Task task) {
		using T = detail::void_fallback_t<typename Task::ResultType>;
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

		if constexpr (std::is_void_v<typename Task::ResultType>)
			return ;
		else
			return std::move(std::get<2>(state.data));
	}

	// Returns std::vector<std::variant<std::exception_ptr, Task::ResultType>>
	template<class Task>
	auto wait_all(std::vector<Task> tasks) {
		detail::vector_wait_all_state_<detail::void_fallback_t<typename Task::ResultType>> state(tasks.size());
		for (std::size_t i=0; i<tasks.size(); i++)
			tasks[i].cb(detail::vector_wait_all_promise_<decltype(state)>{&state, i});

		if (true) {
			std::unique_lock<std::mutex> lk(state.mtx);
			state.cv.wait(lk, [&tasks, &state]() {
				return state.num_finished == tasks.size();
			});
		}

		return std::move(state.datas);
	}

	// Returns std::tuple<std::variant<std::exception_ptr, detail::void_fallback_t<Tasks::ResultType>>...>
	template<class... Tasks>
	auto wait_all(Tasks... tasks) {
		detail::wait_all_state_<detail::void_fallback_t<typename Tasks::ResultType>...> state;
		return detail::wait_all_helper_(std::forward_as_tuple(tasks...), &state, std::make_index_sequence<sizeof...(Tasks)>{});
	}

	template<class... Tasks>
	auto when_all(Tasks... tasks) {
		auto lmbd = [tasks = std::make_tuple(std::move(tasks)...)](auto p) mutable {
			if constexpr (sizeof...(Tasks) == 0) {
				p.set_value(std::tuple<>{});
			}
			else {
				auto state = new detail::when_all_state_<decltype(p), detail::void_fallback_t<typename Tasks::ResultType>...>(std::move(p));
				detail::when_all_helper_(std::move(tasks), std::move(state), std::make_index_sequence<sizeof...(Tasks)>{});
			}
		};

		return detail::typed_task_<std::tuple<std::variant<std::exception_ptr, detail::void_fallback_t<typename Tasks::ResultType>>...>, decltype(lmbd)>{std::move(lmbd)};
	}

	// TODO: wait/when_all_windows to start at most N tasks at once.
	// TODO: async waits on std::vector
};

#endif
