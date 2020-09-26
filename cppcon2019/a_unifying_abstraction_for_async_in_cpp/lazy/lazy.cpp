#include "lazy_future.h"

#include <iostream>

template<typename Task>
auto async_algo(Task task) {
	return lazy::then(std::move(task), [] {
		std::cout << "Calculating..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
		return 42;
	});
}

int main() {
	auto f = async_algo(lazy::new_thread());
	auto f2 = lazy::then(std::move(f), [](int i){
		return i+1;
	});

	std::cout << lazy::sync_wait(std::move(f2)) << std::endl;

	return 0;
}
