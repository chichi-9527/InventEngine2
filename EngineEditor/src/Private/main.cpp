

#include "EditorWindow.h"


#include <IGameModule.h>
#include <ILog.h>
#include <Test.h>

#include <SlangShaderCompiler.h>

int main()
{
    INVENT::ILog::Init({"./Log", "editor"});

    INVENT::ShaderCompiler::Test();

    EditorWindow window;
    window.InitWindow();
    window.Start();
    
    window.Terminate();
    
    getchar();
    return 0;
}
