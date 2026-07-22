#include "Test.h"

#include "ILog.h"
#include "IEngineTools.h"

#include <yaml-cpp/yaml.h>
#include <sol/sol.hpp> 
#include <iostream>

void INVENT_API Test()
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

    INVENT::ILog::Info(INVENT::IEngineTools::GetRunPath());
}
