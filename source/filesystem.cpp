#include "filesystem.hpp"
#include "platform/platform.hpp"
#include "string.hpp"


extern char __rom_end__;


static const char* find_files(Platform& pfrm)
{
    const char* search_start = &__rom_end__;
    const char* search_end = (char*)0x0a000000;

    const char* prefix_str = "core";
    const char* magic = "_filesys";
    const int magic_len = str_len(prefix_str) + str_len(magic);

    const int prefix = *(const int*)prefix_str;

    bool match = false;
    int i = 0;

    for (; i < (search_end - magic_len) - search_start; i += 4) {
        pfrm.feed_watchdog();

        int word;
        memcpy(&word, (search_start + i), 4);

        if (word == prefix) {
            match = true;
            for (int j = 4; j < magic_len; ++j) {
                if (*(search_start + i + j) not_eq magic[j - 4]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                break;
            }
        }
    }

    if (match) {
        return reinterpret_cast<const char*>(search_start + i + magic_len);
    } else {
        return nullptr;
    }
}


Filesystem::Filesystem() : addr_(nullptr)
{
}


bool Filesystem::init(Platform& pfrm)
{
    addr_ = find_files(pfrm);
    return addr_;
}


struct FileInfo {
    char name_[32];
    char size_[16];
    // data[]...
    // null terminator
    // padding (for word alignment)
};


int tonum(const char* str)
{
    int res = 0;

    for (int i = 0; str[i] != '\0'; ++i) {
        res = res * 10 + str[i] - '0';
    }

    return res;
}


Filesystem::FileData Filesystem::get_file(const char* name)
{
    if (not addr_) {
        return {nullptr, 0};
    }
    auto current = reinterpret_cast<const FileInfo*>(addr_);

    while (true) {
        if (str_cmp(name, current->name_) == 0) {
            return {reinterpret_cast<const char*>(current) + sizeof(FileInfo),
                    static_cast<u32>(tonum(current->size_))};
        } else if (current->name_[0] not_eq '\0') {
            auto skip = tonum(current->size_) + 1; // +1 for null terminator
            skip += skip % 4;                      // word padding

            const char* next_addr = reinterpret_cast<const char*>(current) +
                                    sizeof(FileInfo) + skip;

            current = reinterpret_cast<const FileInfo*>(next_addr);

        } else {
            return {nullptr, 0};
        }
    }
}
