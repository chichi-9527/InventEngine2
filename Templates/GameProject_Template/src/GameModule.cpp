#include "IGameModule.h"
#include <iostream>


class MyCoolGame : public IGameModule {
public:
    virtual void BeginPlay() override { std::cout << "遊戲邏輯啟動！" << std::endl; }
    virtual void Tick(float deltaTime) override { /* 遊戲邏輯更新 */ }
    virtual void EndPlay() override { std::cout << "遊戲邏輯關閉！" << std::endl; }
};

extern "C" __declspec(dllexport) IGameModule* CreateGameModule()
{
    return new MyCoolGame();
}
extern "C" __declspec(dllexport) void DestroyGameModule(IGameModule* module)
{
    delete module;
}