// clang-format off

// This file was generated by the project's image build pipeline in
// CMakeLists.txt. I have no idea why cmake outputs semicolons between the
// entries in the IMAGE_INCLUDES list variable... maybe some special formatting
// character that I don't know about... or maybe cmake uses ; internally as a
// delimiter... but for now, the cmake script outputs the semicolons on a new
// line, commented out, to suppress the resulting errors.
//
// If you try to manually edit source/platform/gba/images.cpp, the file may be
// overwritten if you have the GBA_AUTOBUILD_IMG option in the cmake environment
// turned on.



#include "data/overlay.h"
//;
#include "data/overlay_text_key.h"
//;
#include "data/charset.h"
//

struct TextureData {
    const char* name_;
    const unsigned int* tile_data_;
    const unsigned short* palette_data_;
    u32 tile_data_length_;
    u32 palette_data_length_;
};


#define STR(X) #X
#define TEXTURE_INFO(NAME)                                                     \
    {                                                                          \
        STR(NAME), NAME##Tiles, NAME##Pal, NAME##TilesLen, NAME##PalLen        \
    }


static const TextureData sprite_textures[] = {

};


static const TextureData tile_textures[] = {

};


static const TextureData overlay_textures[] = {

    TEXTURE_INFO(overlay),
//;
    TEXTURE_INFO(overlay_text_key),
//;
    TEXTURE_INFO(charset),
//
};

// clang-format on
