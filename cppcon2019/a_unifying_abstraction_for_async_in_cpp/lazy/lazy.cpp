#include "lazy_future.h"

#include <iostream>
#include <memory>

auto async_algo(auto task) {
	return lazy::then(task, []{
		std::cout << "Calculating..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
		return 42;
	});
}

int main() {
	auto f = async_algo(lazy::new_thread());
	auto f2 = then(f, [](int i){
		return i+1;
	});

	std::cout << lazy::sync_wait(f2) << std::endl;

	return 0;
}
