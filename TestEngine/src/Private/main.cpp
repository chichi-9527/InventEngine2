

#include "engine_test.h"
#include "IText.h"

#include <cstdint>
#include <format>
#include <iostream>


int main()
{
    

    INVENT::TestClass test;
    test.Init();

    std::string str("中文");
    
    std::cout << str << "\n";
    
    std::cout << "[Test] 测试完成\n";
    
    getchar();
    return 0;
}