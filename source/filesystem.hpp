#pragma once

#include "platform/platform.hpp"


class Filesystem {
public:
    Filesystem();

    void init(Platform& pfrm);

    const char* get_file(const char* filename);

private:
    const char* addr_;
};
