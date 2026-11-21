#pragma once

#ifdef INVENT_STATIC
    #define INVENT_API
    #define INVENT_DLL
#else

#ifdef _WIN32
    #ifdef EXPORTING_ENGINE
        #define INVENT_API __declspec(dllexport)
        #define TEMPLATE_STRUCT_API template struct __declspec(dllexport)
    #else
        #define INVENT_API __declspec(dllimport)
        #define TEMPLATE_STRUCT_API extern template struct __declspec(dllimport)
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
    #define TEMPLATE_STRUCT_API
#endif // _WIN32

#endif // INVENT_STATIC

#ifdef _WIN32
#ifdef EXPORTING_GAME
#define INVENT_GAME_API __declspec(dllexport)
#else
#define INVENT_GAME_API __declspec(dllimport)
#endif // EXPORTING_ENGINE
//old: #define INVENT_DLL __stdcall, always x64
#define INVENT_GAME_DLL
#else
#if __GNUC__ >= 4
#define INVENT_GAME_API __attribute__((visibility("default")))
#else
#define INVENT_GAME_API
#endif // __GNUC__
#define INVENT_GAME_DLL
#endif // _WIN32

#define IEngine_OUT
#define IEngine_IN

