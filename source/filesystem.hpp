#pragma once

#include "number/int.h"


class Platform;


class Filesystem {
public:
    Filesystem();

    bool init(Platform&);

    struct FileData {
        const char* data_;
        u32 size_;
    };

    FileData get_file(const char* filename);

private:
    const char* addr_;
};
