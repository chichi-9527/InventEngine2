#include "engine_test.h"

#include "slang_c_test.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <assimp/version.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>


namespace INVENT
{
    void TestClass::Init()
    {
         std::cout << "[Engine] 正在初始化 InventEngine..." << std::endl;

    // 1. 測試自己寫的靜態庫 SlangCompiler
    SlangCompiler::TestSlangLink();
    SlangCompiler::TestSlang2();

    // 2. 測試開源靜態庫 (GLFW, Assimp, FreeType)
    if (glfwInit()) {
        std::cout << "[Engine] GLFW 鏈結成功！版本: " << glfwGetVersionString() << std::endl;
        glfwTerminate();
    }
    std::cout << "[Engine] Assimp 鏈結成功！版本: " << aiGetVersionMajor() << "." << aiGetVersionMinor() << std::endl;

    FT_Library ft;
    if (!FT_Init_FreeType(&ft)) {
        std::cout << "[Engine] FreeType 鏈結成功！" << std::endl;
        FT_Done_FreeType(ft);
    }

    // 3. 測試 Vulkan 驅動核心
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::cout << "[Engine] Vulkan 核心鏈結成功！支援擴展數量: " << extensionCount << std::endl;
    }
    
} // namespace INVENT