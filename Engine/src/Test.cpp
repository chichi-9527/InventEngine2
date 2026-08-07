#include "Test.h"

#include "ILog.h"
#include "IEngineTools.h"

#include <yaml-cpp/yaml.h>
#include <sol/sol.hpp> 
#include <yoga/Yoga.h>
#include <iostream>

void Test()
{
    //INVENT::ILog::Init();

    // 2. 測試 yaml-cpp 讀取設定檔 [INDEX_1.4.2]
    YAML::Node config = YAML::Load("{ engine: { name: InventEngine, version: 2.0 } }");
    if (config["engine"]) {
        std::cout << "[Engine] yaml-cpp 讀取成功！引擎名稱: " << config["engine"]["name"].as<std::string>() << std::endl;
    }

    // 3. 測試 sol2 初始化 Lua 虛擬機
    sol::state lua;
    lua.open_libraries(sol::lib::base); // 開啟 Lua 基礎庫
    lua.script("print('[Lua] sol2 綁定成功！這是來自 Lua 腳本的打印。')");

    // test yoga
    YGNodeRef root = YGNodeNew();
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
    YGNodeStyleSetWidth(root, 100.0f);
    YGNodeStyleSetHeight(root, 100.0f);

    YGNodeRef child0 = YGNodeNew();
    YGNodeStyleSetFlexGrow(child0, 1.0f);
    YGNodeStyleSetMargin(child0, YGEdgeRight, 10.0f);
    YGNodeInsertChild(root, child0, 0.0f);

    YGNodeRef child1 = YGNodeNew();
    YGNodeStyleSetFlexGrow(child1, 1.0f);
    YGNodeInsertChild(root, child1, 1.0f);

    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
    float left = YGNodeLayoutGetLeft(child0);
    float height = YGNodeLayoutGetHeight(child0);

    std::cout << "child0 left : " << left << ", height : " << height << std::endl;

    INVENT::ILog::Info(INVENT::IEngineTools::GetRunPath());
}
