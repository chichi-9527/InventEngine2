#include "engine_test.h"

#include "slang_c_test.h"

#include <btBulletDynamicsCommon.h>
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


    // 4. bullet3
    std::cout << "[Engine] 正在配置物理世界..." << std::endl;

    // 建立最基礎的 Bullet 物理世界四件套
    auto* collisionConfiguration = new btDefaultCollisionConfiguration(); //[INDEX_1.2.3]
    auto* dispatcher = new btCollisionDispatcher(collisionConfiguration);
    auto* overlappingPairCache = new btDbvtBroadphase();
    auto* solver = new btSequentialImpulseConstraintSolver;
    auto* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

    // 設置重力 (向下 9.8)
    dynamicsWorld->setGravity(btVector3(0, -9.8f, 0));
    std::cout << "[Engine] Bullet 物理引擎鏈結成功！重力設定完畢。" << std::endl;

    // 清理記憶體
    delete dynamicsWorld;
    delete solver;
    delete overlappingPairCache;
    delete dispatcher;
    delete collisionConfiguration;
    }
    
} // namespace INVENT