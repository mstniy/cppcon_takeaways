#include <utility>
#include <chrono>
#include <thread>
#include <iostream>

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/thread/future.hpp>

boost::future<int> async_algo() {
	boost::promise<int> p;
	boost::future<int> f = p.get_future();

	std::thread t{ [p=std::move(p)]() mutable{
		std::cout << "Calculating..." << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1)); // Working hard
		p.set_value(42);
	}};

	t.detach();
	return f;
}

int main() {
	boost::future<int> f = async_algo().then([](boost::future<int> i){
		return i.get() + 1;
	});

	std::cout << f.get() << std::endl;

	return 0;
}
