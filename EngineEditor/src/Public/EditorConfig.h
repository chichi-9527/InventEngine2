#pragma once


 #ifndef INVENT_EDITOR
 #define INVENT_EDITOR 0
 #endif

#define DevelopmentMode 0

// 开发模式时，更简单的更换是否开启编辑器
#if DevelopmentMode
// #if INVENT_EDITOR
#define WITH_EDITOR 1
#else
#define WITH_EDITOR 0
#endif
