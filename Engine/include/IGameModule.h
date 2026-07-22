#pragma once

#include "EngineAPI.h"

class IGameModule
{
public:
    virtual ~IGameModule() = default;
    virtual void BeginPlay() = 0;
    virtual void Tick(float DeltaTime) = 0;
    virtual void EndPlay() = 0;

};

extern "C"
{
    typedef IGameModule* (*CreateGameModuleFunc)();
    typedef void (*DestroyGameModuleFunc)(IGameModule*);
}