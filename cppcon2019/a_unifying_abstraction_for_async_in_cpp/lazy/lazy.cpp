#include "lazy_future.h"

#include <iostream>

int async_algo_quick() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Working hard
	return 42;
}

int async_algo_slow() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
	return 44;
}

int async_algo_very_slow() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(2)); // Working hard
	return 45;
}

int main() {
	auto f1 = lazy::new_thread(async_algo_quick).then([](int i){
		return i+1;
	});
	auto f2 = lazy::new_thread(async_algo_slow);

	auto f3 = lazy::when_all(std::move(f1), std::move(f2)).then([](auto result){
		std::cout << std::get<1>(std::get<0>(result)) << std::endl;
		std::cout << std::get<1>(std::get<1>(result)) << std::endl;
	});

	auto f4 = lazy::new_thread(async_algo_very_slow).then([](int i){
		std::cout << i << std::endl;
	});

	auto f5 = lazy::when_all(std::move(f3), std::move(f4));

	lazy::wait(std::move(f5));

	return 0;
}
