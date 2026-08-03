
#include <IGameModule.h>
#include <ILog.h>
#include <Test.h>

#include "EditorWindow.h"

int main()
{
    INVENT::ILog::Init();

    EditorWindow window;
    window.InitWindow();
    window.Start();
    
    window.Terminate();

    getchar();
    return 0;
}
