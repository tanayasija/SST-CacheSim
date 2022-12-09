#include <thread>
#include <functional>
#include <iostream>
#include <mutex>
#include <cmath>
#define SIZE 16

// struct My_Int {
// 	int data;
// 	int padding[15];
// };

std::mutex mut;
int num_threads = 4;
int sum[4];
int ocean[SIZE][SIZE];

int foo(int x, int y){
	return ocean[x][y] + ocean[x+1][y] +  ocean[x-1][y] +  ocean[x][y+1] +  ocean[x][y-1];
}

void oceanComp(int threadId) {
	int ndim = (int)std::sqrt(num_threads);
	int tx = threadId / ndim;
	int ty = threadId % ndim;
	int startx = SIZE / ndim  * tx;
	int starty = SIZE / ndim * ty;
	int endx = SIZE / ndim  * (tx+1);
	int endy = SIZE / ndim * (ty+1);

	for (int i = startx; i < endx; i++) {
		for (int j = starty; j < endy; j++) {
			if (i != 0 && j!= 0 && i != SIZE - 1 && j != SIZE - 1) {
				ocean[i][j] = foo(i, j);
			}
		}	
	}
}


int main(){
	// for(int i = 0; i < SIZE; ++i){
	// 	values[i] = i;
	// }

	auto f1 = std::bind(oceanComp, 0);
	auto f2 = std::bind(oceanComp, 1);
	auto f3 = std::bind(oceanComp, 2);
	auto f4 = std::bind(oceanComp, 3);
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
