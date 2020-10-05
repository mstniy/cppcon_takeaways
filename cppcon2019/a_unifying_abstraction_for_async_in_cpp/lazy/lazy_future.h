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
#include <optional>

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

		template<typename>
		struct empty_state{};

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

		template<template<class P> class Substate, class Fun>
		struct then_state_type_alias {
			template<class P>
			using type = Substate<then_promise_<P, Fun>>;
		};

		template<typename T>
		struct wait_state_ {
			std::mutex mtx; // We can avoid these mutexes using c++20's std::atomic::wait, once compilers implement it.
			std::condition_variable cv;
			std::variant<std::monostate, std::exception_ptr, T> data;
		};

		template<typename T>
		struct wait_promise_ {
			wait_state_<T>* pst;

			template<int I, typename... XS> void set(XS&&... xs) {
				std::unique_lock<std::mutex> lk(pst->mtx); // We need this lock here because the waiting thread might spuriusly wake up and check pst->data
				pst->data.template emplace<I>(std::forward<XS>(xs)...);
				pst->cv.notify_one();
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<2>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<1>(e);}
		};

		template<typename P, typename Task>
		struct vector_when_all_state_base_ {
			std::optional<P> p;	// The promise to be called when all tasks finish
						// TODO: Can we avoid std::optional?
			std::vector<std::variant<std::exception_ptr, detail::void_fallback_t<typename Task::ResultType>>> datas;
			std::atomic<std::size_t> num_finished{0};
		};

		template<typename MultiWaitStateBase>
		struct vector_when_all_promise_ {
			MultiWaitStateBase* pst;
			std::size_t index;

			vector_when_all_promise_(MultiWaitStateBase* state, std::size_t index):
				pst(state),
				index(index)
			{}
			vector_when_all_promise_(const vector_when_all_promise_&) = delete;
			vector_when_all_promise_(vector_when_all_promise_&& o) = default;

			template<int I, typename... XS> void set(XS&&... xs) {
				pst->datas[index].template emplace<I>(std::forward<XS>(xs)...);
				std::size_t finish_id = pst->num_finished.fetch_add(1)+1;
				if (finish_id == pst->datas.size()) { // Last task to end calls the promise
					pst->p->set_value(std::move(pst->datas));
				}
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<1>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<0>(e);}
		};

		template<typename P, typename Task>
		struct vector_when_all_state_ {
			vector_when_all_state_base_<P, Task> base;
			std::vector<typename Task::template StateType<vector_when_all_promise_<decltype(base)>>> substates;

			void resize(std::size_t size) {
				substates.resize(size);
				base.datas.resize(size);
			}
		};

		template<class Task>
		struct vector_when_all_state_alias {
			template<typename P>
			using type = vector_when_all_state_<P, Task>;
		};

		template<typename P, typename... Tasks>
		struct when_all_state_base_ {
			std::optional<P> p;	// The promise to be called when all tasks finish
						// TODO: Can we avoid std::optional?
			std::tuple<std::variant<std::exception_ptr, detail::void_fallback_t<typename Tasks::ResultType>>...> datas;
			std::atomic<std::size_t> num_finished{0};
			static constexpr std::size_t num_tasks = sizeof...(Tasks);
		};

		template<typename MultiWaitStateBase, std::size_t I>
		struct when_all_promise_ {
			MultiWaitStateBase* pst;

			when_all_promise_(MultiWaitStateBase* pst):pst(pst){}

			when_all_promise_(const when_all_promise_&) = delete;
			when_all_promise_(when_all_promise_&& o) = default;

			template<int J, typename... XS> void set(XS&&... xs) {
				std::get<I>(pst->datas).template emplace<J>(std::forward<XS>(xs)...); // This is NOT a race. Different tasks (on different threads) write to different memory locations.
				std::size_t finish_id = pst->num_finished.fetch_add(1)+1;
				if (finish_id == pst->num_tasks) { // Last task to end calls the promise
					pst->p->set_value(std::move(pst->datas));
				}
			}

			template<typename... VS>
			void set_value(VS&&... vs) {set<1>(std::forward<VS>(vs)...);}
			template<typename E>
			void set_exception(E e) {set<0>(e);}
		};

		// This is not an actual function. It represents a type transform, so that we can properly set the type of the *substates* field in *when_all_state_*.
		template<typename P, typename... Tasks, std::size_t... Indices>
		auto when_all_state_substate_type_transform_helper_(std::index_sequence<Indices...>) ->
		std::tuple<typename Tasks::template StateType<when_all_promise_<when_all_state_base_<P, Tasks...>, Indices>>...>;


		template<typename P, typename... Tasks>
		struct when_all_state_ {
			when_all_state_base_<P, Tasks...> base;
			decltype(when_all_state_substate_type_transform_helper_<P, Tasks...>(std::make_index_sequence<sizeof...(Tasks)>{})) substates;
		};

		template<class... Tasks>
		struct when_all_state_alias {
			template<typename P>
			using type = when_all_state_<P, Tasks...>;
		};

		template<std::size_t Index, class Task, class MultiWaitState>
		void when_all_start_task_(Task task, MultiWaitState* state, std::size_t task_count, bool may_block)
		{
			task.cb(when_all_promise_<std::remove_reference_t<decltype(state->base)>, Index>{&state->base}, &std::get<Index>(state->substates), may_block && (Index == task_count-1));
		}

		template<class Tasks, class MultiWaitState, std::size_t... Indices>
		void when_all_helper_(Tasks tasks, MultiWaitState* state, bool may_block, std::index_sequence<Indices...>)
		{
			(..., when_all_start_task_<Indices>(std::move(std::get<Indices>(tasks)), state, std::tuple_size_v<Tasks>, may_block));
		}

		template<typename Result, typename Callback, template<typename P> class State = detail::empty_state>
		struct typed_task_
		{
			using ResultType = Result;

			template<typename P>
			using StateType = State<P>;

			Callback cb; // Signature: void(Promise, State<Promise>*, bool may_block)

			template<typename Fun>
			auto then(Fun fun) {
				auto lmbd = [cb=std::move(cb), fun=std::move(fun)](auto p, auto* state, bool may_block) mutable{
					cb(detail::then_promise_<decltype(p), Fun>{std::move(p), std::move(fun)}, state, may_block);
				};
				if constexpr (std::is_void_v<Result>){
					using FunResult = std::result_of_t<Fun&&()>;
					return detail::typed_task_<FunResult, decltype(lmbd), then_state_type_alias<StateType, Fun>::template type>{std::move(lmbd)};
				}
				else {
					using FunResult = std::result_of_t<Fun&&(Result&&)>;
					return detail::typed_task_<FunResult, decltype(lmbd), then_state_type_alias<StateType, Fun>::template type>{std::move(lmbd)};
				}
			}
		};
	};

	template<typename Fun>
	auto new_thread(Fun fun) {
		auto lmbd =  [](auto p, auto* /*state*/, bool may_block){
			if (may_block) { // If we are allowed to block (i.e., use the current thread), set the promise on the current thread
				p.set_value();
			}
			else { // Otherwise, create a new one
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
			}
		};
		return detail::typed_task_<void, decltype(lmbd)>{std::move(lmbd)}.then(std::move(fun));
	}

	template<class Task>
	auto wait(Task task) {
		using TaskResult = detail::void_fallback_t<typename Task::ResultType>;
		detail::wait_state_<TaskResult> wait_state;
		detail::wait_promise_<TaskResult> promise{&wait_state};
		typename Task::template StateType<decltype(promise)> task_state;
		task.cb(std::move(promise), &task_state, true);

		if (true) {
			std::unique_lock<std::mutex> lk(wait_state.mtx);
			wait_state.cv.wait(lk, [&wait_state]{
				return wait_state.data.index() != 0;
			});
		}

		if (wait_state.data.index() == 1)
			std::rethrow_exception(std::get<1>(wait_state.data));

		if constexpr (std::is_void_v<typename Task::ResultType>)
			return ;
		else
			return std::move(std::get<2>(wait_state.data));
	}

	// The returned task completes with a std::vector<std::variant<std::exception_ptr, detail::void_fallback_t<Tasks::ResultType>>>
	template<class Task>
	auto when_all(std::vector<Task> tasks) {
		using TaskResult = std::vector<std::variant<std::exception_ptr, detail::void_fallback_t<typename Task::ResultType>>>;

		auto lmbd = [tasks = std::move(tasks)](auto p, auto* state, bool may_block) mutable {
			if (tasks.size() == 0) {
				p.set_value(TaskResult{});
			}
			else {
				state->base.p.emplace(std::move(p));
				state->resize(tasks.size());

				for (std::size_t i=0; i<tasks.size(); i++)
					tasks[i].cb(detail::vector_when_all_promise_<std::remove_reference_t<decltype(state->base)>>{&state->base, i}, &state->substates[i], may_block && (i == tasks.size()-1)); // Can run the last task on the current thread
			}
		};

		return detail::typed_task_<TaskResult, decltype(lmbd), detail::vector_when_all_state_alias<Task>::template type>{std::move(lmbd)};
	}

	// The returned task completes with a std::tuple<std::variant<std::exception_ptr, detail::void_fallback_t<Tasks::ResultType>>...>
	template<class... Tasks>
	auto when_all(Tasks... tasks) {
		auto lmbd = [tasks = std::make_tuple(std::move(tasks)...)](auto p, auto* state, bool may_block) mutable {
			if constexpr (sizeof...(Tasks) == 0) {
				p.set_value(std::tuple<>{});
			}
			else {
				state->base.p.emplace(std::move(p));
				detail::when_all_helper_(std::move(tasks), state, may_block, std::make_index_sequence<sizeof...(Tasks)>{});
			}
		};

		using TaskResult = std::tuple<std::variant<std::exception_ptr, detail::void_fallback_t<typename Tasks::ResultType>>...>;
		return detail::typed_task_<TaskResult, decltype(lmbd), detail::when_all_state_alias<Tasks...>::template type>{std::move(lmbd)};
	}

	template<class... Tasks>
	auto wait_all(Tasks... tasks) {
		return wait(when_all(tasks...));
	}

	// TODO: when_all_windowed to start at most N tasks at once.
};

#endif
