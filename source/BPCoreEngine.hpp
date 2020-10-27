#pragma once


#include "platform/platform.hpp"
#include "filesystem.hpp"

extern "C" {
#include "lua/lua.h"
}


class BPCoreEngine {
public:
    BPCoreEngine(Platform& pf);

    void run();

private:
    lua_State* lua_;
    Filesystem fs_;
};
