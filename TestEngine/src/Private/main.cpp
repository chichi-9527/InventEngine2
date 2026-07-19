

#include "engine_test.h"
#include "IText.h"

#include <cstdint>
#include <format>
#include <iostream>


int main()
{
    

    INVENT::TestClass test;
    test.Init();

    INVENT::IText text1("中文 字符串 string");
    INVENT::IText text2(u8"中文 utf8 字符串 u8string");

    auto utf8 = text1.ToUtf8();
    std::cout << text1.ToUtf8() << std::endl;
    std::cout << text2.ToUtf8() << std::endl;

    char s1[6];
    auto s1_size = INVENT::IText::ToUtf8FromUInt32(text1[0], s1);

    std::cout << "size: " << s1_size << ", str: " << s1 << std::endl;
    std::cout << "size: " << std::strlen(s1) << std::endl;
    
    std::cout << "[Test] 测试完成\n";
    
    getchar();
    return 0;
}