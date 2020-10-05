#include "lazy_future.h"

#include <iostream>

auto async_algo(int ms, int res) {
	return [ms, res](){
		std::cout << "Calculating..." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(ms)); // Working hard
		std::cout << "Done..." << std::endl;
		return res;
	};
}

int main() {
	auto f1 = lazy::new_thread(async_algo(500, 42)).then([](int i){
		return i+1;
	});
	auto f2 = lazy::new_thread(async_algo(1000, 44));

	auto f3 = lazy::when_all(std::move(f1), std::move(f2)).then([](auto result){
		std::cout << std::get<1>(std::get<0>(result)) << std::endl;
		std::cout << std::get<1>(std::get<1>(result)) << std::endl;
	});

	auto f4 = lazy::new_thread(async_algo(2000, 45)).then([](int i){
		std::cout << i << std::endl;
	});

	auto f5 = lazy::when_all(std::move(f3), std::move(f4));

	lazy::wait(std::move(f5));

	return 0;
}
