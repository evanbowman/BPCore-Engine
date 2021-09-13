#pragma once

#include "number/numeric.hpp"
#include "string.hpp"



class TileDataStream {
public:

    virtual ~TileDataStream() {}


    virtual bool read(u16* output) = 0;


    virtual bool skip(int cells) = 0;


    virtual bool next_row() = 0;

};



class CSVTileDataStream : public TileDataStream {
public:

    CSVTileDataStream(const char* data, u32 len) :
        data_(data),
        len_(len)
    {
    }


    bool read(u16* output) override
    {
        if (pos_ == len_) {
            return false;
        }

        if (data_[pos_] == '\r') {
            // Some people use MS Windows for some reason
            ++pos_;
        }

        if (data_[pos_] == '\n') {
            ++pos_;
            x_ = 0;
            ++y_;
        }

        StringBuffer<20> buffer;

        while (data_[pos_] not_eq ',' and
               data_[pos_] not_eq '\n' and
               data_[pos_] not_eq '\r') {
            if (pos_ == len_) {
                break;
            }
            buffer.push_back(data_[pos_++]);
        }

        if (data_[pos_] == ',') {
            ++pos_;
        }
        ++x_;


        if (output) {
            *output = std::atoi(buffer.c_str());
        }

        return true;
    }


    bool next_row() override
    {
        while (true) {
            if (pos_ == len_) {
                return false;
            }

            if (data_[pos_] == '\r') {
                ++pos_;
            } else if (data_[pos_] == '\n') {
                ++pos_;
                x_ = 0;
                ++y_;
                return true;
            } else {
                ++pos_;
            }
        }
    }


    bool skip(int cells) override
    {
        for (int i = 0; i < cells; ++i) {
            if (not read(nullptr)) {
                return false;
            }
        }

        return true;
    }


private:
    const char* data_;
    u32 len_;
    u32 pos_ = 0;
    int x_ = 0;
    int y_ = 0;
};
