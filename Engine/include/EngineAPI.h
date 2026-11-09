#pragma once

#ifdef INVENT_STATIC
    #define INVENT_API
    #define INVENT_DLL
#else

#ifdef _WIN32
    #ifdef EXPORTING_ENGINE
        #define INVENT_API __declspec(dllexport)
    #else
        #define INVENT_API __declspec(dllimport)
    #endif // EXPORTING_ENGINE
    //old: #define INVENT_DLL __stdcall, always x64
    #define INVENT_DLL
#else
    #if __GNUC__ >= 4
        #define INVENT_API __attribute__((visibility("default")))
    #else
        #define INVENT_API
    #endif // __GNUC__
    #define INVENT_DLL
#endif // _WIN32

#endif // INVENT_STATIC

#define IEngine_OUT
#define IEngine_IN



