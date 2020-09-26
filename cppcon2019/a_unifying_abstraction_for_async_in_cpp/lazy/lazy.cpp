#include "lazy_future.h"

#include <iostream>

auto async_algo_quick() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Working hard
	return 42;
}

auto async_algo_slow() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
	return 5;
}

int main() {
	auto f1 = lazy::new_thread().then(async_algo_quick).then([](auto i){
		return i+1;
	});
	auto f2 = lazy::new_thread().then(async_algo_slow);

	//auto result = lazy::sync_wait_all(std::move(f1), std::move(f2));

	auto result = lazy::sync_wait_any(std::move(f2), std::move(f1));
	std::cout << result.first << std::endl;

	return 0;
}
