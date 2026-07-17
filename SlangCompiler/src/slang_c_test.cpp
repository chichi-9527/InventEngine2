#include "slang_c_test.h"

#include <slang.h>
#include <slang-com-helper.h>
#include <slang-com-ptr.h>
#include <iostream>

namespace SlangCompiler
{
    void TestSlangLink()
    {
        Slang::ComPtr<slang::IGlobalSession> globalSession;
        SlangGlobalSessionDesc desc{};
        desc.enableGLSL = true;
        auto result = createGlobalSession(&desc, globalSession.writeRef());
        if (!SLANG_SUCCEEDED(result))
        {
            std::cerr << "[SlangCompiler] Failed to create global session: " << result << std::endl;
            return;
        }
        std::cout << "[SlangCompiler] Global session created successfully." << std::endl;
    }

    void TestSlang2()
    {
        std::cout << "[SlangCompiler] Test." << std::endl;
    }

} // namespace SlangCompiler