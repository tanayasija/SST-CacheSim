#include <thread>
#include <functional>
#include <iostream>
#include <mutex>
#define SIZE 128

struct My_Int {
	int data;
	int padding[15];
};

std::mutex mut;
My_Int sum[4];
int values[SIZE];

void foo(int startingPos, int endPos, int offset){
	while(startingPos < endPos){
		// mut.lock();
		sum[offset].data += values[startingPos++];
		// mut.unlock();
	}
}



int main(){
	for(int i = 0; i < SIZE; ++i){
		values[i] = i;
	}

	auto f1 = std::bind(foo, 0, SIZE/4, 0);
	auto f2 = std::bind(foo, SIZE/4, SIZE/2, 1);
	auto f3 = std::bind(foo, SIZE/2, SIZE/4 * 3, 2);
	auto f4 = std::bind(foo, SIZE/4 * 3, SIZE, 3);
	std::thread t1(f1);
	std::thread t2(f2);
	std::thread t3(f3);
	std::thread t4(f4);
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	// long finalSum = 0;
	// for(int i = 0; i < 4; ++i){
	//  	finalSum += sum[i].data;
	// }
	// std::cout<<"finalSum:"<<finalSum<<std::endl;
	// std::cout<<"addr of sum: "<<&sum<<std::endl;
}
