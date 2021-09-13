#pragma once


#include "filesystem.hpp"
#include "platform/platform.hpp"

extern "C" {
#include "lua/lua.h"
}


class BPCoreEngine {
public:
    BPCoreEngine(Platform& pf);

    void run();

private:
    lua_State* lua_;
};
