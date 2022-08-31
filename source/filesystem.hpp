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

    FileData get_file(int address, int len);
    FileData next_file(int address, int len);

private:
    const char* addr_;
};
