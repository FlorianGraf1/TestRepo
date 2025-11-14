#include <windows.h>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <vector>
#include <conio.h>

CRITICAL_SECTION cs;
int counter[50] = { 0 };

class MyClass {
public:
    MyClass(int testInt) : _testInt(testInt) {}

private:
    int _testInt;
};

int increment() {

    MyClass* testClass = new MyClass(5);

    int index = std::rand() % 100;

    EnterCriticalSection(&cs);
           
        int* p = nullptr;
        *p = 42;

    LeaveCriticalSection(&cs);
    
	delete testClass;
	return 1;
}

void unusedFunction() {
    std::cout << "This is an unused function." << std::endl;
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
