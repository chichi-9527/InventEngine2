#pragma once

#ifdef INVENT_STATIC
    #define INVENT_API
#else

#ifdef _WIN32
    #ifdef EXPORTING_ENGINE
        #define INVENT_API __declspec(dllexport)
    #else
        #define INVENT_API __declspec(dllimport)
    #endif // EXPORTING_ENGINE

#else
    #if __GNUC__ >= 4
        #define INVENT_API __attribute__((visibility("default")))
    #else
        #define INVENT_API
    #endif // __GNUC__
#endif // _WIN32

#endif // INVENT_STATIC




