//#include "engine_pch.h"
#include "engine_test.h"

#include "ILog.h"

#include "slang_c_test.h"

#include <btBulletDynamicsCommon.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <assimp/version.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <format>


namespace INVENT
{
    void TestClass::Init()
    {
        ILog::Init();

        ILog::Info("[Engine] 正在初始化 InventEngine...");

        // 1. 測試自己寫的靜態庫 SlangCompiler
        SlangCompiler::TestSlangLink();
        SlangCompiler::TestSlang2();

        // 2. 測試開源靜態庫 (GLFW, Assimp, FreeType)
        if (glfwInit()) {
            ILog::Info(std::format("[Engine] GLFW 鏈結成功！版本: {}", glfwGetVersionString()));
            glfwTerminate();
        }
        ILog::Info(std::format("[Engine] Assimp 鏈結成功！版本: {}.{}", aiGetVersionMajor(), aiGetVersionMinor()));

        FT_Library ft;
        if (!FT_Init_FreeType(&ft)) 
        {
            ILog::Info("[Engine] FreeType 鏈結成功！");
            FT_Done_FreeType(ft);
        }

        // 3. 測試 Vulkan 驅動核心
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        ILog::Info(std::format("[Engine] Vulkan 核心鏈結成功！支援擴展數量: ", extensionCount));


        // 4. bullet3
        ILog::Info("[Engine] 正在配置物理世界...");

        // 建立最基礎的 Bullet 物理世界四件套
        auto* collisionConfiguration = new btDefaultCollisionConfiguration(); //[INDEX_1.2.3]
        auto* dispatcher = new btCollisionDispatcher(collisionConfiguration);
        auto* overlappingPairCache = new btDbvtBroadphase();
        auto* solver = new btSequentialImpulseConstraintSolver;
        auto* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);

        // 設置重力 (向下 9.8)
        dynamicsWorld->setGravity(btVector3(0, -9.8f, 0));
        ILog::Info("[Engine] Bullet 物理引擎鏈結成功！重力設定完畢。");

        // 清理記憶體
        delete dynamicsWorld;
        delete solver;
        delete overlappingPairCache;
        delete dispatcher;
        delete collisionConfiguration;


        // 
        std::vector<int> vec1;
    
        ILog::Warning("Warning");
        ILog::Error("Error");
        ILog::Debug("Debug");
        ILog::Trace("Trace");
        ILog::Fatal("Fatal");

    }
    
} // namespace INVENT