#include "lazy_future.h"

#include <iostream>

auto async_algo() {
	std::cout << "Calculating..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
	return 42;
}

int main() {
	auto f = lazy::new_thread().then(async_algo).then([](auto i){
		return i+1;
	});

	std::cout << lazy::sync_wait(std::move(f)) << std::endl;

	return 0;
}
