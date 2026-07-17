

#include "engine_test.h"

#include <iostream>



int main()
{
    std::cout << "[Test] 啟動測試程序 (C++20)" << std::endl;

    INVENT::TestClass test;
    test.Init();
    
    std::cout << "[Test] 測試程序順利結束！" << std::endl;
    return 0;
}