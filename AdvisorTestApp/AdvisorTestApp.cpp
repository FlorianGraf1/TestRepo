#include <windows.h>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <vector>
#include <conio.h>

CRITICAL_SECTION cs;
int counter[50] = { 0 };

void increment() {
        int index = std::rand() % 100;
//        for (int i = 0; i < 100000; ++i) {
            EnterCriticalSection(&cs);
           
           int* p = nullptr;
           *p = 42;
            //throw 1;
            LeaveCriticalSection(&cs);
//        }
        EnterCriticalSection(&cs);
        counter[index] = -1;
        LeaveCriticalSection(&cs);
    
}



int main() {
    InitializeCriticalSection(&cs);

    while (true) {
        std::vector<std::thread> threads;

        for (int i = 0; i < 50; ++i) {
            threads.emplace_back(increment);
        }

        for (auto& t : threads) {
            t.join();
        }
    }
    DeleteCriticalSection(&cs);
}
