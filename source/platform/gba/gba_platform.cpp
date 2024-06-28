
////////////////////////////////////////////////////////////////////////////////
//
//
// Gameboy Advance Platform
//
//
////////////////////////////////////////////////////////////////////////////////

#ifdef __GBA__

#include "bulkAllocator.hpp"
#include "graphics/overlay.hpp"
#include "images.cpp"
#include "number/random.hpp"
#include "platform/platform.hpp"
#include "string.hpp"
#include "umm_malloc/src/umm_malloc.h"
#include "util.hpp"
#include <algorithm>


void english__to_string(int num, char* buffer, int base);


#include "gba.h"


struct BiosVersion {
    enum {
        NDS = static_cast<long unsigned int>(-1162995584),
        GBA = static_cast<long unsigned int>(-1162995585)
    };
};


Platform::DeviceName Platform::device_name() const
{
    switch (BiosCheckSum()) {
    case BiosVersion::NDS:
        return "NintendoDS";

    case BiosVersion::GBA:
        return "GameboyAdvance";

    default:
        return "Unknown";
    }
}


void Platform::enable_feature(const char* feature_name, bool enabled)
{
    // ...
}


// These word and halfword versions of memcpy are written in assembly. They use
// special ARM instructions to copy data faster than you could do with thumb
// code.
extern "C" {
__attribute__((section(".iwram"), long_call)) void
memcpy32(void* dst, const void* src, uint wcount);
void memcpy16(void* dst, const void* src, uint hwcount);
}


////////////////////////////////////////////////////////////////////////////////
//
// Tile Memory Layout:
//
// The game uses every single available screen block, so the data is fairly
// tightly packed. Here's a chart representing the layout:
//
// All units of length are in screen blocks, followed by the screen block
// indices in parentheses. The texture data needs to be aligned to char block
// boundaries (eight screen blocks in a char block), which is why there is
// tilemap data packed into the screen blocks between sets of texture data.
//
//        charblock 0                      charblock 1
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// o============================================================
// |  t0 texture   | overlay mem |   t1 texture   |   bg mem   |
// | len 7 (0 - 6) |  len 1 (7)  | len 7 (8 - 14) | len 1 (15) | ...
// o============================================================
//
//        charblock 2                 charblock 3
//     ~~~~~~~~~~~~~~~~~~ ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//     ======================================================o
//     | overlay texture |     t0 mem      |     t1 mem      |
// ... | len 8 (16 - 23) | len 4 (24 - 27) | len 4 (28 - 31) |
//     ======================================================o
//

static constexpr const int sbb_per_cbb = 8; // ScreenBaseBlock per CharBaseBlock

static constexpr const int sbb_overlay_tiles = 7;
static constexpr const int sbb_bg_tiles = 15;
static constexpr const int sbb_t0_tiles = 24;
static constexpr const int sbb_t1_tiles = 28;

static constexpr const int sbb_overlay_texture = 16;
static constexpr const int sbb_t0_texture = 0;
static constexpr const int sbb_t1_texture = 8;
static constexpr const int sbb_bg_texture = sbb_t0_texture;

static constexpr const int cbb_overlay_texture =
    sbb_overlay_texture / sbb_per_cbb;

static constexpr const int cbb_t0_texture = sbb_t0_texture / sbb_per_cbb;
static constexpr const int cbb_t1_texture = sbb_t1_texture / sbb_per_cbb;
static constexpr const int cbb_bg_texture = sbb_bg_texture / sbb_per_cbb;


//
//
////////////////////////////////////////////////////////////////////////////////


void start(Platform&);


static Platform* platform;


[[gnu::used]] alignas(4) static EWRAM_DATA u8 heap[240000];

void* UMM_MALLOC_CFG_HEAP_ADDR = &heap;
uint32_t UMM_MALLOC_CFG_HEAP_SIZE = sizeof heap;


int main(int argc, char** argv)
{
    umm_init();

    Platform pf;
    ::platform = &pf;

    start(pf);
}


const char* Platform::get_opt(char opt)
{
    // Command line arguments aren't supported, seeing we are running without
    // an operating system.
    return nullptr;
}


////////////////////////////////////////////////////////////////////////////////
// DeltaClock
////////////////////////////////////////////////////////////////////////////////


Platform::DeltaClock::DeltaClock() : impl_(nullptr)
{
}


static size_t delta_total;


static int delta_read_tics()
{
    return REG_TM3CNT_L + delta_total;
}


static Microseconds delta_convert_tics(int tics)
{
    //
    // IMPORTANT: Already well into development, I discovered that the Gameboy
    // Advance does not refresh at exactly 60 frames per second. Rather than
    // change all of the code, I am going to keep the timestep as-is. Anyone
    // porting the code to a new platform should make the appropriate
    // adjustments in their implementation of DeltaClock. I believe the actual
    // refresh rate on the GBA is something like 59.59.
    //
    // P.S.: Now, I've discovered that the screen refresh rate is actually 59.73
    // Hz. Sorry to have created a headache for anyone in the future who may be
    // attempting to port this game.
    //
    return ((tics * (59.59f / 60.f)) * 60.f) / 1000.f;
}


Platform::DeltaClock::TimePoint Platform::DeltaClock::sample() const
{
    return delta_read_tics();
}


Microseconds Platform::DeltaClock::duration(TimePoint t1, TimePoint t2)
{
    return delta_convert_tics(t2 - t1);
}


Microseconds Platform::DeltaClock::reset()
{
    // (1 second / 60 frames) x (1,000,000 microseconds / 1 second) =
    // 16,666.6...

    irqDisable(IRQ_TIMER3);
    const auto tics = delta_read_tics();
    REG_TM3CNT_H = 0;

    irqEnable(IRQ_TIMER3);

    delta_total = 0;

    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = 1 << 7 | 1 << 6;

    return delta_convert_tics(tics);
}


Platform::DeltaClock::~DeltaClock()
{
}


////////////////////////////////////////////////////////////////////////////////
// Keyboard
////////////////////////////////////////////////////////////////////////////////


static volatile u32* keys = (volatile u32*)0x04000130;


void Platform::Keyboard::register_controller(const ControllerInfo& info)
{
    // ...
}


void Platform::Keyboard::poll()
{
    std::copy(std::begin(states_), std::end(states_), std::begin(prev_));

    states_[int(Key::action_1)] = ~(*keys) & KEY_A;
    states_[int(Key::action_2)] = ~(*keys) & KEY_B;
    states_[int(Key::start)] = ~(*keys) & KEY_START;
    states_[int(Key::select)] = ~(*keys) & KEY_SELECT;
    states_[int(Key::right)] = ~(*keys) & KEY_RIGHT;
    states_[int(Key::left)] = ~(*keys) & KEY_LEFT;
    states_[int(Key::down)] = ~(*keys) & KEY_DOWN;
    states_[int(Key::up)] = ~(*keys) & KEY_UP;
    states_[int(Key::alt_1)] = ~(*keys) & KEY_L;
    states_[int(Key::alt_2)] = ~(*keys) & KEY_R;
}


////////////////////////////////////////////////////////////////////////////////
// Screen
////////////////////////////////////////////////////////////////////////////////


struct alignas(4) ObjectAttributes {
    u16 attribute_0;
    u16 attribute_1;
    u16 attribute_2;

    s16 affine_transform;
};


// See documentation. Object memory provides thirty-two matrices for affine
// transformation; the parameters nestled between every four objects.
struct alignas(4) ObjectAffineMatrix {
    ObjectAttributes o0;
    ObjectAttributes o1;
    ObjectAttributes o2;
    ObjectAttributes o3;

    auto& pa()
    {
        return o0.affine_transform;
    }
    auto& pb()
    {
        return o1.affine_transform;
    }
    auto& pc()
    {
        return o2.affine_transform;
    }
    auto& pd()
    {
        return o3.affine_transform;
    }

    void identity()
    {
        pa() = 0x0100l;
        pb() = 0;
        pc() = 0;
        pd() = 0x0100;
    }

    void scale(s16 sx, s16 sy)
    {
        pa() = (1 << 8) - sx;
        pb() = 0;
        pc() = 0;
        pd() = (1 << 8) - sy;
    }

    void rotate(s16 degrees)
    {
        // I have no recollection of why the shift by seven works. I saw some
        // libraries shift by four, but that seemed not to work from what I
        // remember. Everyone seems to use a different sine lookup table, might
        // be the culprit.
        const int ss = sine(degrees) >> 7;
        const int cc = cosine(degrees) >> 7;

        pa() = cc;
        pb() = -ss;
        pc() = ss;
        pd() = cc;
    }

    void rot_scale(s16 degrees, s16 x, s16 y)
    {
        // FIXME: This code doesn't seem to work correctly yet...
        const int ss = sine(degrees);
        const int cc = cosine(degrees);

        pa() = cc * x >> 12;
        pb() = -ss * x >> 12;
        pc() = ss * y >> 12;
        pd() = cc * y >> 12;
    }
};


namespace attr0_mask {
constexpr u16 disabled{2 << 8};
}


constexpr u32 oam_count = Platform::Screen::sprite_limit;


static ObjectAttributes* const object_attribute_memory = {
    (ObjectAttributes*)0x07000000};

static ObjectAttributes
    object_attribute_back_buffer[Platform::Screen::sprite_limit];


static auto affine_transform_back_buffer =
    reinterpret_cast<ObjectAffineMatrix*>(object_attribute_back_buffer);


static const u32 affine_transform_limit = 32;
static u32 affine_transform_write_index = 0;
static u32 last_affine_transform_write_index = 0;


static volatile u16* bg0_control = (volatile u16*)0x4000008;
static volatile u16* bg1_control = (volatile u16*)0x400000a;
static volatile u16* bg2_control = (volatile u16*)0x400000c;
static volatile u16* bg3_control = (volatile u16*)0x400000e;


static volatile short* bg0_x_scroll = (volatile short*)0x4000010;
static volatile short* bg0_y_scroll = (volatile short*)0x4000012;
static volatile short* bg1_x_scroll = (volatile short*)0x4000014;
static volatile short* bg1_y_scroll = (volatile short*)0x4000016;
static volatile short* bg2_x_scroll = (volatile short*)0x4000018;
static volatile short* bg2_y_scroll = (volatile short*)0x400001a;
static volatile short* bg3_x_scroll = (volatile short*)0x400001c;
static volatile short* bg3_y_scroll = (volatile short*)0x400001e;


static u8 last_fade_amt;
static ColorConstant last_color;
static bool last_fade_include_sprites;


static volatile u16* reg_blendcnt = (volatile u16*)0x04000050;
static volatile u16* reg_blendalpha = (volatile u16*)0x04000052;

#define BLD_BUILD(top, bot, mode)                                              \
    ((((bot)&63) << 8) | (((mode)&3) << 6) | ((top)&63))
#define BLD_OBJ 0x0010
#define BLD_BG0 0x0001
#define BLD_BG1 0x0002
#define BLD_BG3 0x0008
#define BLDA_BUILD(eva, evb) (((eva)&31) | (((evb)&31) << 8))


#include "gba_color.hpp"


static int sprite_priority = 1;


void Platform::set_priorities(int sprite_prior,
                              int background_prior,
                              int tile0_prior,
                              int tile1_prior)
{
    screen().init_layers(background_prior, tile0_prior, tile1_prior);
    ::sprite_priority = sprite_prior;
}


int frame_stall_count = 0;
int vblank_count = 0;


void Platform::Screen::set_frame_stalls(int stall_count)
{
    frame_stall_count = stall_count;
}


void Platform::Screen::init_layers(int background_prior,
                                   int tile0_prior,
                                   int tile1_prior)
{
    // Tilemap layer 0
    *bg0_control = BG_CBB(cbb_t0_texture) | BG_SBB(sbb_t0_tiles) |
                   BG_REG_64x64 | BG_PRIORITY(tile0_prior) | BG_MOSAIC;

    // Tilemap layer 1
    *bg3_control = BG_CBB(cbb_t1_texture) | BG_SBB(sbb_t1_tiles) |
                   BG_REG_64x64 | BG_PRIORITY(tile1_prior) | BG_MOSAIC;

    // The starfield background
    *bg1_control = BG_CBB(cbb_bg_texture) | BG_SBB(sbb_bg_tiles) |
                   BG_PRIORITY(background_prior) | BG_MOSAIC;

    // The overlay
    *bg2_control = BG_CBB(cbb_overlay_texture) | BG_SBB(sbb_overlay_tiles) |
                   BG_PRIORITY(0) | BG_MOSAIC;
}


Platform::Screen::Screen() : userdata_(nullptr)
{
    REG_DISPCNT = MODE_0 | OBJ_ENABLE | OBJ_MAP_1D | BG0_ENABLE | BG1_ENABLE |
                  BG2_ENABLE | BG3_ENABLE;

    *reg_blendcnt = BLD_BUILD(BLD_OBJ, BLD_BG0 | BLD_BG1 | BLD_BG3, 0);

    *reg_blendalpha = BLDA_BUILD(0x40 / 8, 0x40 / 8);

    init_layers(3, 3, 2);

    view_.set_size(this->size().cast<Float>());

    REG_MOSAIC = MOS_BUILD(0, 0, 1, 1);
}


static u32 last_oam_write_index = 0;
static u32 oam_write_index = 0;


Color real_color(ColorConstant k)
{
    switch (k) {
    case ColorConstant::electric_blue:
        static const Color el_blue(0, 31, 31);
        return el_blue;

    case ColorConstant::turquoise_blue:
        static const Color turquoise_blue(0, 31, 27);
        return turquoise_blue;

    case ColorConstant::cerulean_blue:
        static const Color cerulean_blue(12, 27, 31);
        return cerulean_blue;

    case ColorConstant::picton_blue:
        static const Color picton_blue(9, 20, 31);
        return picton_blue;

    case ColorConstant::maya_blue:
        static const Color maya_blue(10, 23, 31);
        return maya_blue;

    case ColorConstant::aged_paper:
        static const Color aged_paper(27, 24, 18);
        return aged_paper;

    case ColorConstant::silver_white:
        static const Color silver_white(29, 29, 30);
        return silver_white;

    case ColorConstant::rich_black:
        static const Color rich_black(0, 0, 2);
        return rich_black;

    default:
        return Color(k);
    }
}


using PaletteBank = int;
constexpr PaletteBank available_palettes = 3;
constexpr PaletteBank palette_count = 16;

static PaletteBank palette_counter = available_palettes;


static u8 screen_pixelate_amount = 0;


Color adjust_warmth(const Color& c, int amount)
{
    auto ret = c;
    ret.r_ = clamp(c.r_ + amount, 0, 31);
    ret.b_ = clamp(c.b_ - amount, 0, 31);

    return ret;
}


static auto blend(const Color& c1, const Color& c2, u8 amt)
{
    switch (amt) {
    case 0:
        return c1.bgr_hex_555();
    case 255:
        return c2.bgr_hex_555();
    default:
        return Color(fast_interpolate(c2.r_, c1.r_, amt),
                     fast_interpolate(c2.g_, c1.g_, amt),
                     fast_interpolate(c2.b_, c1.b_, amt))
            .bgr_hex_555();
    }
}


static bool night_mode = false;


static Color nightmode_adjust(const Color& c)
{
    if (not night_mode) {
        return c;
    } else {
        return adjust_warmth(
            Color::from_bgr_hex_555(blend(c, c.grayscale(), 190)), 2);
    }
}


// For the purpose of saving cpu cycles. The color_mix function scans a list of
// previous results, and if one matches the current blend parameters, the caller
// will set the locked_ field to true, and return the index of the existing
// palette bank. Each call to display() unlocks all of the palette infos.
static struct PaletteInfo {
    ColorConstant color_ = ColorConstant::null;
    u8 blend_amount_ = 0;
    bool locked_ = false;
} palette_info[palette_count] = {};


// We want to be able to disable color mixes during a screen fade. We perform a
// screen fade by blending a color into the base palette. If we allow sprites to
// use other palette banks during a screen fade, they won't be faded, because
// they are not using the first palette bank.
static bool color_mix_disabled = false;


// Perform a color mix between the spritesheet palette bank (bank zero), and
// return the palette bank where the resulting mixture is stored. We can only
// display 12 mixed colors at a time, because the first four banks are in use.
static PaletteBank color_mix(ColorConstant k, u8 amount)
{
    if (UNLIKELY(color_mix_disabled)) {
        return 0;
    }

    for (PaletteBank palette = available_palettes; palette < 16; ++palette) {
        auto& info = palette_info[palette];
        if (info.color_ == k and info.blend_amount_ == amount) {
            info.locked_ = true;
            return palette;
        }
    }

    // Skip over any palettes that are in use
    while (palette_info[palette_counter].locked_) {
        if (UNLIKELY(palette_counter == palette_count)) {
            return 0;
        }
        ++palette_counter;
    }

    if (UNLIKELY(palette_counter == palette_count)) {
        return 0; // Exhausted all the palettes that we have for effects.
    }

    const auto c = nightmode_adjust(real_color(k));

    if (amount not_eq 255) {
        for (int i = 0; i < 16; ++i) {
            auto from = Color::from_bgr_hex_555(MEM_PALETTE[i]);
            const u32 index = 16 * palette_counter + i;
            MEM_PALETTE[index] = Color(fast_interpolate(c.r_, from.r_, amount),
                                       fast_interpolate(c.g_, from.g_, amount),
                                       fast_interpolate(c.b_, from.b_, amount))
                                     .bgr_hex_555();
        }
    } else {
        for (int i = 0; i < 16; ++i) {
            const u32 index = 16 * palette_counter + i;
            // No need to actually perform the blend operation if we're mixing
            // in 100% of the other color.
            MEM_PALETTE[index] = c.bgr_hex_555();
        }
    }

    palette_info[palette_counter] = {k, amount, true};

    return palette_counter++;
}


void Platform::Screen::draw(const Sprite& spr)
{
    if (UNLIKELY(spr.get_alpha() == Sprite::Alpha::transparent)) {
        return;
    }

    const auto& mix = spr.get_mix();

    const auto pb = [&]() -> PaletteBank {
        if (UNLIKELY(mix.color_ not_eq ColorConstant::null)) {
            if (const auto pal_bank = color_mix(mix.color_, mix.amount_)) {
                return ATTR2_PALBANK(pal_bank);
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }();

    auto draw_sprite = [&](int tex_off, int x_off, int scale) {
        if (UNLIKELY(oam_write_index == oam_count)) {
            return;
        }
        const auto position =
            spr.get_position().cast<s32>() - spr.get_origin().cast<s32>();

        const auto view_center = view_.get_center().cast<s32>();

        auto abs_position = position - view_center;
        if (abs_position.x < -16 or abs_position.x > 256 or
            abs_position.y < -16 or abs_position.y > 176) {
            return;
        }

        auto oa = object_attribute_back_buffer + oam_write_index;
        if (spr.get_alpha() not_eq Sprite::Alpha::translucent) {
            oa->attribute_0 = ATTR0_COLOR_16 | ATTR0_SQUARE;
        } else {
            oa->attribute_0 = ATTR0_COLOR_16 | ATTR0_SQUARE | ATTR0_BLEND;
        }
        oa->attribute_1 = ATTR1_SIZE_16; // clear attr1

        oa->attribute_0 &= (0xff00 & ~((1 << 8) | (1 << 9))); // clear attr0

        if (spr.get_rotation() or spr.get_scale().x or spr.get_scale().y) {
            if (affine_transform_write_index not_eq affine_transform_limit) {
                auto& affine =
                    affine_transform_back_buffer[affine_transform_write_index];

                if (spr.get_rotation() and
                    (spr.get_scale().x or spr.get_scale().y)) {
                    affine.rot_scale(spr.get_rotation(),
                                     spr.get_scale().x,
                                     spr.get_scale().y);
                } else if (spr.get_rotation()) {
                    affine.rotate(spr.get_rotation());
                } else {
                    affine.scale(spr.get_scale().x, spr.get_scale().y);
                }

                oa->attribute_0 |= 1 << 8;
                oa->attribute_0 |= 1 << 9;

                abs_position.x -= 8;
                abs_position.y -= 16;

                oa->attribute_1 |= affine_transform_write_index << 9;

                affine_transform_write_index += 1;
            }
        } else {
            const auto& flip = spr.get_flip();
            oa->attribute_1 |= ((int)flip.y << 13);
            oa->attribute_1 |= ((int)flip.x << 12);
        }

        oa->attribute_0 |= abs_position.y & 0x00ff;

        if ((mix.amount_ > 215 and mix.amount_ < 255) or
            screen_pixelate_amount not_eq 0) {

            oa->attribute_0 |= ATTR0_MOSAIC;
        }

        oa->attribute_1 |= (abs_position.x + x_off) & 0x01ff;
        oa->attribute_2 = 2 + spr.get_texture_index() * scale + tex_off;
        oa->attribute_2 |= pb;
        oa->attribute_2 |= ATTR2_PRIORITY(::sprite_priority);
        oam_write_index += 1;
    };

    switch (spr.get_size()) {
    case Sprite::Size::w16_h16:
        draw_sprite(0, 0, 4);
        break;
    }
}


static Buffer<Platform::Task*, 7> task_queue_;


void Platform::push_task(Task* task)
{
    task->complete_ = false;
    task->running_ = true;

    if (not task_queue_.push_back(task)) {
        error(*this, "failed to enqueue task");
        while (true)
            ;
    }
}


void Platform::Screen::clear()
{
    // On the GBA, we don't have real threads, so run tasks prior to the vsync,
    // so any updates are least likely to cause tearing.
    for (auto it = task_queue_.begin(); it not_eq task_queue_.end();) {
        (*it)->run();
        if ((*it)->complete()) {
            (*it)->running_ = false;
            task_queue_.erase(it);
        } else {
            ++it;
        }
    }

    if (vblank_count < frame_stall_count) {
        VBlankIntrWait();
    }

    // VSync
    VBlankIntrWait();

    vblank_count = 0;
}


static void key_wake_isr();
static void key_standby_isr();
static bool enter_sleep = false;


static void key_wake_isr()
{
    REG_KEYCNT = KEY_SELECT | KEY_R | KEY_L | KEYIRQ_ENABLE | KEYIRQ_AND;

    irqSet(IRQ_KEYPAD, key_standby_isr);
}


static void key_standby_isr()
{
    REG_KEYCNT = KEY_SELECT | KEY_START | KEY_A | KEY_B | DPAD | KEYIRQ_ENABLE |
                 KEYIRQ_OR;

    irqSet(IRQ_KEYPAD, key_wake_isr);

    enter_sleep = true;
}


static ScreenBlock overlay_back_buffer alignas(u32);
static bool overlay_back_buffer_changed = false;


u16 t1_scroll_x = 0;
u16 t1_scroll_y = 0;
u16 t0_scroll_x = 0;
u16 t0_scroll_y = 0;
u16 bg_scroll_x = 0;
u16 bg_scroll_y = 0;


void Platform::scroll(Layer layer, u16 xscroll, u16 yscroll)
{
    switch (layer) {
    case Layer::overlay:
        *bg2_x_scroll = xscroll;
        *bg2_y_scroll = yscroll;
        break;

    case Layer::map_1:
        t1_scroll_x = xscroll;
        t1_scroll_y = yscroll;
        break;

    case Layer::map_0:
        t0_scroll_x = xscroll;
        t0_scroll_y = yscroll;
        break;

    case Layer::background:
        bg_scroll_x = xscroll;
        bg_scroll_y = yscroll;
        break;
    }
}


void Platform::Screen::display()
{
    // platform->stopwatch().start();

    if (UNLIKELY(enter_sleep)) {
        enter_sleep = false;
        if (not ::platform->network_peer().is_connected()) {
            ::platform->sleep(180);
            Stop();
        }
    }

    if (overlay_back_buffer_changed) {
        overlay_back_buffer_changed = false;

        memcpy32(MEM_SCREENBLOCKS[sbb_overlay_tiles],
                 overlay_back_buffer,
                 (sizeof overlay_back_buffer) / 4);
    }

    for (u32 i = oam_write_index; i < last_oam_write_index; ++i) {
        // Disable affine transform for unused sprite
        object_attribute_back_buffer[i].attribute_0 &= ~((1 << 8) | (1 << 9));
        object_attribute_back_buffer[i].attribute_1 = 0;

        object_attribute_back_buffer[i].attribute_0 |= attr0_mask::disabled;
    }

    for (u32 i = affine_transform_write_index;
         i < last_affine_transform_write_index;
         ++i) {

        auto& affine = affine_transform_back_buffer[i];
        affine.pa() = 0;
        affine.pb() = 0;
        affine.pc() = 0;
        affine.pd() = 0;
    }

    // I noticed less graphical artifacts when using a back buffer. I thought I
    // would see better performance when writing directly to OAM, rather than
    // doing a copy later, but I did not notice any performance difference when
    // adding a back buffer.
    memcpy32(object_attribute_memory,
             object_attribute_back_buffer,
             (sizeof object_attribute_back_buffer) / 4);

    last_affine_transform_write_index = affine_transform_write_index;
    affine_transform_write_index = 0;

    last_oam_write_index = oam_write_index;
    oam_write_index = 0;
    palette_counter = available_palettes;

    for (auto& info : palette_info) {
        info.locked_ = false;
    }

    auto view_offset = view_.get_center().cast<s32>();
    *bg0_x_scroll = view_offset.x + t0_scroll_x;
    *bg0_y_scroll = view_offset.y + t0_scroll_y;

    *bg3_x_scroll = view_offset.x + t1_scroll_x;
    *bg3_y_scroll = view_offset.y + t1_scroll_y;

    *bg1_x_scroll = view_offset.x + bg_scroll_x;
    *bg1_y_scroll = view_offset.y + bg_scroll_y;

    // // Depending on the amount of the background scroll, we want to mask off
    // // certain parts of bg0 and bg3. The background tiles wrap when they scroll
    // // a certain distance, and wrapping looks strange (although it might be
    // // useful if you were making certain kinds of games, like some kind of
    // // Civilization clone, but for BlindJump, it doesn't make sense to display
    // // the wrapped area).
    // const s32 scroll_limit_x_max = 512 - size().x;
    // const s32 scroll_limit_y_max = 480 - size().y;
    // if (view_offset.x > scroll_limit_x_max) {
    //     REG_WIN0H =
    //         (0 << 8) | (size().x - (view_offset.x - scroll_limit_x_max));
    // } else if (view_offset.x < 0) {
    //     REG_WIN0H = ((view_offset.x * -1) << 8) | (0);
    // } else {
    //     REG_WIN0H = (0 << 8) | (size().x);
    // }

    // if (view_offset.y > scroll_limit_y_max) {
    //     REG_WIN0V =
    //         (0 << 8) | (size().y - (view_offset.y - scroll_limit_y_max));
    // } else if (view_offset.y < 0) {
    //     REG_WIN0V = ((view_offset.y * -1) << 8) | (0);
    // } else {
    //     REG_WIN0V = (0 << 8) | (size().y);
    // }
}


Vec2<u32> Platform::Screen::size() const
{
    static const Vec2<u32> gba_widescreen{240, 160};
    return gba_widescreen;
}


////////////////////////////////////////////////////////////////////////////////
// Platform
////////////////////////////////////////////////////////////////////////////////


static const TextureData* current_spritesheet = &sprite_textures[0];
static const TextureData* current_tilesheet0 = &tile_textures[0];
static const TextureData* current_tilesheet1 = &tile_textures[1];
static const TextureData* current_overlay_texture = &overlay_textures[1];

static u16 sprite_palette[16];
static u16 tilesheet_0_palette[16];
static u16 tilesheet_1_palette[16];
static u16 overlay_palette[16];


// We use base_contrast as the starting point for all contrast calculations. In
// most screen modes, the base contrast will be zero, but in some situations,
// like when we have night mode enabled, the base contrast will be decreased,
// and then further contrast adjustments will be calculated according to the
// shifted base value.
static Contrast base_contrast = 0;
static Contrast contrast = 0;


Contrast Platform::Screen::get_contrast() const
{
    return ::contrast;
}


static void
init_palette(const TextureData* td, u16* palette, bool skip_contrast)
{
    const auto adj_cr = contrast + base_contrast;

    for (int i = 0; i < 16; ++i) {
        if (not skip_contrast and adj_cr not_eq 0) {
            const Float f = (259.f * (adj_cr + 255)) / (255 * (259 - adj_cr));
            const auto c =
                nightmode_adjust(Color::from_bgr_hex_555(td->palette_data_[i]));

            const auto r =
                clamp(f * (Color::upsample(c.r_) - 128) + 128, 0.f, 255.f);
            const auto g =
                clamp(f * (Color::upsample(c.g_) - 128) + 128, 0.f, 255.f);
            const auto b =
                clamp(f * (Color::upsample(c.b_) - 128) + 128, 0.f, 255.f);

            palette[i] = Color(Color::downsample(r),
                               Color::downsample(g),
                               Color::downsample(b))
                             .bgr_hex_555();

        } else {
            palette[i] =
                nightmode_adjust(Color::from_bgr_hex_555(td->palette_data_[i]))
                    .bgr_hex_555();
        }
    }
}


void Platform::Screen::enable_night_mode(bool enabled)
{
    ::night_mode = enabled;

    if (::night_mode) {
        ::base_contrast = -12;
    } else {
        ::base_contrast = 0;
    }

    init_palette(current_spritesheet, sprite_palette, false);
    init_palette(current_tilesheet0, tilesheet_0_palette, false);
    init_palette(current_tilesheet1, tilesheet_1_palette, false);
    init_palette(current_overlay_texture, overlay_palette, true);

    // TODO: Edit code so that we don't need a specific hack here for the
    // overlay palette.
    for (int i = 0; i < 16; ++i) {
        MEM_BG_PALETTE[16 + i] = overlay_palette[i];
    }
}


void Platform::Screen::set_contrast(Contrast c)
{
    ::contrast = c;

    init_palette(current_spritesheet, sprite_palette, false);
    init_palette(current_tilesheet0, tilesheet_0_palette, false);
    init_palette(current_tilesheet1, tilesheet_1_palette, false);
    init_palette(current_overlay_texture, overlay_palette, true);
}


static u32 validate_tilemap_texture_size(Platform& pfrm, size_t size)
{
    constexpr auto charblock_size = sizeof(ScreenBlock) * 7;
    if (size > charblock_size) {
        return size - charblock_size;
    }
    return 0;
}


static u32 validate_overlay_texture_size(Platform& pfrm, size_t size)
{
    constexpr auto charblock_size = sizeof(ScreenBlock) * 8;
    if (size > charblock_size) {
        return size - charblock_size;
    }
    return 0;
}


u16 Platform::get_tile(Layer layer, u16 x, u16 y)
{
    switch (layer) {
    case Layer::overlay:
        if (x > 31 or y > 31) {
            return 0;
        }
        return overlay_back_buffer[x + y * 32] & ~(SE_PALBANK_MASK);

    case Layer::background:
        if (x > 31 or y > 31) {
            return 0;
        }
        return MEM_SCREENBLOCKS[sbb_bg_tiles][x + y * 32];

    case Layer::map_0:
        if (x > 63 or y > 63) {
            return 0;
        }
        if (x < 32 and y < 32) {
            return MEM_SCREENBLOCKS[sbb_t0_tiles][x + y * 32];
        } else if (y < 32) {
            return MEM_SCREENBLOCKS[sbb_t0_tiles + 1][(x - 32) + y * 32];
        } else if (x < 32) {
            return MEM_SCREENBLOCKS[sbb_t0_tiles + 2][x + (y - 32) * 32];
        } else {
            return MEM_SCREENBLOCKS[sbb_t0_tiles + 3][(x - 32) + (y - 32) * 32];
        }
        break;

    case Layer::map_1:
        if (x > 63 or y > 63) {
            return 0;
        }
        if (x < 32 and y < 32) {
            return MEM_SCREENBLOCKS[sbb_t1_tiles][x + y * 32];
        } else if (y < 32) {
            return MEM_SCREENBLOCKS[sbb_t1_tiles + 1][(x - 32) + y * 32];
        } else if (x < 32) {
            return MEM_SCREENBLOCKS[sbb_t1_tiles + 2][x + (y - 32) * 32];
        } else {
            return MEM_SCREENBLOCKS[sbb_t1_tiles + 3][(x - 32) + (y - 32) * 32];
        }
    }
    return 0;
}


[[noreturn]] static void restart()
{
    RegisterRamReset(RESET_VRAM);
    SoftReset(ROM_RESTART), __builtin_unreachable();
}


void Platform::fatal()
{
    restart();
}


void Platform::set_overlay_origin(Float x, Float y)
{
}


// Screen fades are cpu intensive. We want to skip any work that we possibly
// can.
static bool overlay_was_faded = false;


// TODO: May be possible to reduce tearing by deferring the fade until the
// Screen::display() call...
void Platform::Screen::fade(float amount,
                            ColorConstant k,
                            std::optional<ColorConstant> base,
                            bool include_sprites,
                            bool include_overlay)
{
    const u8 amt = amount * 255;

    if (amt < 128) {
        color_mix_disabled = false;
    } else {
        color_mix_disabled = true;
    }

    if (amt == last_fade_amt and k == last_color and
        last_fade_include_sprites == include_sprites) {
        return;
    }

    last_fade_amt = amt;
    last_color = k;
    last_fade_include_sprites = include_sprites;

    const auto c = nightmode_adjust(real_color(k));

    if (not base) {
        for (int i = 0; i < 16; ++i) {
            auto from = Color::from_bgr_hex_555(sprite_palette[i]);
            MEM_PALETTE[i] = blend(from, c, include_sprites ? amt : 0);
        }
        for (int i = 0; i < 16; ++i) {
            auto from = Color::from_bgr_hex_555(tilesheet_0_palette[i]);
            MEM_BG_PALETTE[i] = blend(from, c, amt);
        }
        for (int i = 0; i < 16; ++i) {
            auto from = Color::from_bgr_hex_555(tilesheet_1_palette[i]);
            MEM_BG_PALETTE[32 + i] = blend(from, c, amt);
        }
        if (include_overlay or overlay_was_faded) {
            for (int i = 0; i < 16; ++i) {
                auto from = Color::from_bgr_hex_555(overlay_palette[i]);
                MEM_BG_PALETTE[16 + i] =
                    blend(from, c, include_overlay ? amt : 0);
            }
        }
        overlay_was_faded = include_overlay;
    } else {
        const auto bc = nightmode_adjust(real_color(*base));
        for (int i = 0; i < 16; ++i) {
            MEM_PALETTE[i] = blend(bc, c, include_sprites ? amt : 0);
            MEM_BG_PALETTE[i] = blend(bc, c, amt);
            MEM_BG_PALETTE[32 + i] = blend(bc, c, amt);

            if (overlay_was_faded) {
                // FIXME!
                for (int i = 0; i < 16; ++i) {
                    auto from = Color::from_bgr_hex_555(overlay_palette[i]);
                    MEM_BG_PALETTE[16 + i] = blend(from, c, 0);
                }
                overlay_was_faded = false;
            }
        }
    }
}


void Platform::Screen::pixelate(u8 amount,
                                bool include_overlay,
                                bool include_background,
                                bool include_sprites)
{
    screen_pixelate_amount = amount;

    if (amount == 0) {
        REG_MOSAIC = MOS_BUILD(0, 0, 1, 1);
    } else {
        REG_MOSAIC = MOS_BUILD(amount >> 4,
                               amount >> 4,
                               include_sprites ? amount >> 4 : 0,
                               include_sprites ? amount >> 4 : 0);

        if (include_overlay) {
            *bg2_control |= BG_MOSAIC;
        } else {
            *bg2_control &= ~BG_MOSAIC;
        }

        if (include_background) {
            *bg0_control |= BG_MOSAIC;
            *bg1_control |= BG_MOSAIC;
        } else {
            *bg0_control &= ~BG_MOSAIC;
            *bg1_control &= ~BG_MOSAIC;
        }
    }
}


static u16 spritesheet_source_pal[16];
static TextureData spritesheet_file_data;


static std::optional<Platform::FailureReason>
push_spritesheet_texture(const TextureData& info)
{
    current_spritesheet = &info;

    init_palette(current_spritesheet, sprite_palette, false);

    const auto obj_vram_size = 1024 * 32;

    if (info.tile_data_length_ > obj_vram_size) {

        const auto exceeded_bytes = info.tile_data_length_ - obj_vram_size;

        Platform::FailureReason r;
        r += "exceeded sprite vram capacity by ";

        const auto bytes_per_tile = 32;
        const auto bytes_per_sprite = bytes_per_tile * 4;

        char buffer[32];
        english__to_string(exceeded_bytes / bytes_per_sprite, buffer, 10);

        r += buffer;
        r += " tile(s).";

        return r;
    }

    // NOTE: There are four tile blocks, so index four points to the
    // end of the tile memory.
    memcpy16(
        (void*)&MEM_TILE[4][1], info.tile_data_, info.tile_data_length_ / 2);

    // We need to do this, otherwise whatever screen fade is currently
    // active will be overwritten by the copy.
    const auto c = nightmode_adjust(real_color(last_color));
    for (int i = 0; i < 16; ++i) {
        auto from = Color::from_bgr_hex_555(sprite_palette[i]);
        MEM_PALETTE[i] = blend(from, c, last_fade_amt);
    }

    return {};
}


std::optional<Platform::FailureReason>
Platform::load_sprite_texture(const char* name, int addr, int len)
{
    StringBuffer<48> palette_file(name);
    palette_file += ".pal";


    const auto img = addr ? fs().get_file(addr, len) : fs().get_file(name);
    const auto palette = addr ? fs().next_file(addr, len) : fs().get_file(palette_file.c_str());

    if (img.data_ and palette.data_) {
        memcpy(spritesheet_source_pal,
               palette.data_,
               sizeof spritesheet_source_pal);

        TextureData& info = spritesheet_file_data;
        info.name_ = name;
        info.tile_data_ = (const unsigned int*)img.data_;
        info.palette_data_ = spritesheet_source_pal;
        info.tile_data_length_ = img.size_;
        info.palette_data_length_ = 16;

        return push_spritesheet_texture(info);
    }

    for (auto& info : sprite_textures) {

        if (str_cmp(name, info.name_) == 0) {

            return push_spritesheet_texture(info);
        }
    }

    FailureReason r;
    r += "missing ";
    r += name;
    r += " or ";
    r += palette_file;
    r += ".";

    return r;
}


static u16 tile0_source_pal[16];
static TextureData tile0_file_data;


static std::optional<Platform::FailureReason>
push_tile0_texture(const char* name, const TextureData& info)
{
    current_tilesheet0 = &info;

    init_palette(current_tilesheet0, tilesheet_0_palette, false);


    // We don't want to load the whole palette into memory, we might
    // overwrite palettes used by someone else, e.g. the overlay...
    //
    // Also, like the sprite texture, we need to apply the currently
    // active screen fade while modifying the color palette.
    const auto c = nightmode_adjust(real_color(last_color));
    for (int i = 0; i < 16; ++i) {
        auto from = Color::from_bgr_hex_555(tilesheet_0_palette[i]);
        MEM_BG_PALETTE[i] = blend(from, c, last_fade_amt);
    }

    auto exceeded_bytes =
        validate_tilemap_texture_size(*platform, info.tile_data_length_);

    if (not exceeded_bytes) {
        memcpy16((void*)&MEM_SCREENBLOCKS[sbb_t0_texture][0],
                 info.tile_data_,
                 info.tile_data_length_ / 2);
        return {};
    } else {
        Platform::FailureReason r;
        r += "exceeded tile0 vram capacity by ";

        char buffer[32];
        english__to_string(exceeded_bytes / 32, buffer, 10);

        r += buffer;
        r += " tile(s).";

        return r;
    }
}


std::optional<Platform::FailureReason>
Platform::load_tile0_texture(const char* name, int addr, int len)
{
    StringBuffer<48> palette_file(name);
    palette_file += ".pal";

    const auto img = addr ? fs().get_file(addr, len) : fs().get_file(name);
    const auto palette = addr ? fs().next_file(addr, len) : fs().get_file(palette_file.c_str());

    if (img.data_ and palette.data_) {

        memcpy(tile0_source_pal, palette.data_, sizeof tile0_source_pal);

        TextureData& info = tile0_file_data;
        info.name_ = name;
        info.tile_data_ = (const unsigned int*)img.data_;
        info.palette_data_ = tile0_source_pal;
        info.tile_data_length_ = img.size_;
        info.palette_data_length_ = 16;

        return push_tile0_texture(name, info);
    }

    for (auto& info : tile_textures) {

        if (str_cmp(name, info.name_) == 0) {

            return push_tile0_texture(name, info);
        }
    }

    FailureReason r;
    r += "missing ";
    r += name;
    r += " or ";
    r += palette_file;
    r += ".";

    return r;
}


static u16 tile1_source_pal[16];
static TextureData tile1_file_data;


static std::optional<Platform::FailureReason>
push_tile1_texture(const char* name, const TextureData& info)
{
    current_tilesheet1 = &info;

    init_palette(current_tilesheet1, tilesheet_1_palette, false);

    // We don't want to load the whole palette into memory, we might
    // overwrite palettes used by someone else, e.g. the overlay...
    //
    // Also, like the sprite texture, we need to apply the currently
    // active screen fade while modifying the color palette.
    const auto c = nightmode_adjust(real_color(last_color));
    for (int i = 0; i < 16; ++i) {
        auto from = Color::from_bgr_hex_555(tilesheet_1_palette[i]);
        MEM_BG_PALETTE[32 + i] = blend(from, c, last_fade_amt);
    }

    auto exceeded_bytes =
        validate_tilemap_texture_size(*platform, info.tile_data_length_);

    if (not exceeded_bytes) {
        memcpy16((void*)&MEM_SCREENBLOCKS[sbb_t1_texture][0],
                 info.tile_data_,
                 info.tile_data_length_ / 2);
        return {};
    } else {
        Platform::FailureReason r;
        r += "exceeded tile1 vram capacity by ";

        char buffer[32];
        english__to_string(exceeded_bytes / 32, buffer, 10);

        r += buffer;
        r += " tile(s).";

        return r;
    }
}


std::optional<Platform::FailureReason>
Platform::load_tile1_texture(const char* name, int addr, int len)
{
    StringBuffer<48> palette_file(name);
    palette_file += ".pal";

    const auto img = addr ? fs().get_file(addr, len) : fs().get_file(name);
    const auto palette = addr ? fs().next_file(addr, len) : fs().get_file(palette_file.c_str());

    if (img.data_ and palette.data_) {

        memcpy(tile1_source_pal, palette.data_, sizeof tile1_source_pal);

        TextureData& info = tile1_file_data;
        info.name_ = name;
        info.tile_data_ = (const unsigned int*)img.data_;
        info.palette_data_ = tile1_source_pal;
        info.tile_data_length_ = img.size_;
        info.palette_data_length_ = 16;

        return push_tile1_texture(name, info);
    }

    for (auto& info : tile_textures) {

        if (str_cmp(name, info.name_) == 0) {

            return push_tile1_texture(name, info);
        }
    }

    FailureReason r;
    r += "missing ";
    r += name;
    r += " or ";
    r += palette_file;
    r += ".";

    return r;
}


void Platform::sleep(u32 frames)
{
    // NOTE: A sleep call should just pause the game for some number of frames,
    // but doing so should not have an impact on delta timing
    // calculation. Therefore, we need to stop the hardware timer associated
    // with the delta clock, and zero out the clock's contents when finished
    // with the sleep cycles.

    irqDisable(IRQ_TIMER3);

    auto old_vbl = vblank_count;
    while (frames--) {
        VBlankIntrWait();
    }
    vblank_count = old_vbl;

    irqEnable(IRQ_TIMER3);
}


bool Platform::is_running() const
{
    // Unlike the windowed desktop platform, as long as the device is
    // powered on, the game is running.
    return true;
}


static byte* const cartridge_ram = (byte*)0x0E000000;


static bool
flash_byteverify(void* in_dst, const void* in_src, unsigned int length)
{
    unsigned char* src = (unsigned char*)in_src;
    unsigned char* dst = (unsigned char*)in_dst;

    for (; length > 0; length--) {

        if (*dst++ != *src++)
            return true;
    }
    return false;
}


static void
flash_bytecpy(void* in_dst, const void* in_src, unsigned int length, bool write)
{
    unsigned char* src = (unsigned char*)in_src;
    unsigned char* dst = (unsigned char*)in_dst;

    for (; length > 0; length--) {
        if (write) {
            *(volatile u8*)0x0E005555 = 0xAA;
            *(volatile u8*)0x0E002AAA = 0x55;
            *(volatile u8*)0x0E005555 = 0xA0;
        }
        *dst++ = *src++;
    }
}


static void set_flash_bank(u32 bankID)
{
    if (bankID < 2) {
        *(volatile u8*)0x0E005555 = 0xAA;
        *(volatile u8*)0x0E002AAA = 0x55;
        *(volatile u8*)0x0E005555 = 0xB0;
        *(volatile u8*)0x0E000000 = bankID;
    }
}

// FIXME: Lets be nice to the flash an not write to the same memory
// location every single time! What about a list? Each new save will
// have a unique id. On startup, scan through memory until you reach
// the highest unique id. Then start writing new saves at that
// point. NOTE: My everdrive uses SRAM for Flash writes anyway, so
// it's probably not going to wear out, but I like to pretend that I'm
// developing a real gba game.


COLD static bool flash_save(const void* data, u32 flash_offset, u32 length)
{
    if ((u32)flash_offset >= 0x10000) {
        set_flash_bank(1);
    } else {
        set_flash_bank(0);
    }

    flash_bytecpy((void*)(cartridge_ram + flash_offset), data, length, true);

    return flash_byteverify(
        (void*)(cartridge_ram + flash_offset), data, length);
}


static void flash_load(void* dest, u32 flash_offset, u32 length)
{
    if (flash_offset >= 0x10000) {
        set_flash_bank(1);
    } else {
        set_flash_bank(0);
    }

    flash_bytecpy(
        dest, (const void*)(cartridge_ram + flash_offset), length, false);
}


static bool save_using_flash = false;


// NOTE: Some cartridge manufacturers back in the day searched ROMS for a
// word-aligned string, to determine what type of save memory to put on the
// chip. I designed the code to use either SRAM or FLASH, but let's include the
// backup ID string anyway, because we'd really prefer to have SRAM. Unlikely
// that anyone would ever agree to make me a GBA cartridge, but hey, you never
// know...
READ_ONLY_DATA alignas(4) [[gnu::used]] static const
    char backup_type[] = {'S', 'R', 'A', 'M', '_', 'V', 'n', 'n', 'n'};


void sram_save(const void* data, u32 offset, u32 length)
{
    u8* save_mem = (u8*)cartridge_ram + offset;

    // The cartridge has an 8-bit bus, so you have to write one byte at a time,
    // otherwise it won't work!
    for (size_t i = 0; i < length; ++i) {
        *save_mem++ = ((const u8*)data)[i];
    }
}


void sram_load(void* dest, u32 offset, u32 length)
{
    u8* save_mem = (u8*)cartridge_ram + offset;
    for (size_t i = 0; i < length; ++i) {
        ((u8*)dest)[i] = *save_mem++;
    }
}


bool Platform::write_save_data(const void* data, u32 length)
{
    if (save_using_flash) {
        return flash_save(data, 0, length);
    } else {
        sram_save(data, 0, length);
        return true;
    }
}


bool Platform::read_save_data(void* buffer, u32 data_length)
{
    if (save_using_flash) {
        flash_load(buffer, 0, data_length);
    } else {
        sram_load(buffer, 0, data_length);
    }
    return true;
}


void SynchronizedBase::init(Platform& pf)
{
}


void SynchronizedBase::lock()
{
}


void SynchronizedBase::unlock()
{
}


SynchronizedBase::~SynchronizedBase()
{
}


////////////////////////////////////////////////////////////////////////////////
// Logger
////////////////////////////////////////////////////////////////////////////////


// This is unfortunate. Maybe we should define a max save data size as part of
// the platform header, so that we do not need to pull in game specific code.
#include "persistentData.hpp"


static const u32 initial_log_write_loc = 32000 - 16;
static u32 log_write_loc = initial_log_write_loc;

#define REG_DEBUG_ENABLE (volatile u16*)0x4FFF780
#define REG_DEBUG_FLAGS (volatile u16*)0x4FFF700
#define REG_DEBUG_STRING (char*)0x4FFF600
#define MGBA_LOG_DEBUG 4


int mgba_detect()
{
    *REG_DEBUG_ENABLE = 0xC0DE;
    return *REG_DEBUG_ENABLE == 0x1DEA;
}


Platform::Logger::Logger()
{
}


static Severity log_threshold;


void Platform::Logger::set_threshold(Severity severity)
{
    log_threshold = severity;
}


void Platform::Logger::log(Severity level, const char* msg)
{
    if (mgba_detect()) {
        auto len = str_len(msg);
        if (len > 0x100) {
            len = 0x100;
        }
        for (u32 i = 0; i < len; ++i) {
            (REG_DEBUG_STRING)[i] = msg[i];
        }
        *REG_DEBUG_FLAGS = MGBA_LOG_DEBUG | 0x100;
    }
}


void Platform::Logger::read(void* buffer, u32 start_offset, u32 num_bytes)
{
    if (save_using_flash) {
        flash_load(buffer, sizeof(PersistentData) + start_offset, num_bytes);
    } else {
        sram_load(buffer, sizeof(PersistentData) + start_offset, num_bytes);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Speaker
//
// For music, the Speaker class uses the GameBoy's direct sound chip to play
// 8-bit signed raw audio, at 16kHz.
//
////////////////////////////////////////////////////////////////////////////////


void Platform::Speaker::play_note(Note n, Octave o, Channel c)
{
}


static const int null_music_len = 8;
static const u32 null_music[null_music_len] = {0, 0, 0, 0, 0, 0, 0, 0};


#define DEF_AUDIO(__STR_NAME__, __TRACK_NAME__, __DIV__)                       \
    {                                                                          \
        STR(__STR_NAME__), (AudioSample*)__TRACK_NAME__,                       \
            __TRACK_NAME__##Len / __DIV__                                      \
    }


#define DEF_MUSIC(__STR_NAME__, __TRACK_NAME__)                                \
    DEF_AUDIO(__STR_NAME__, __TRACK_NAME__, 4)


#define DEF_SOUND(__STR_NAME__, __TRACK_NAME__)                                \
    DEF_AUDIO(__STR_NAME__, __TRACK_NAME__, 1)


#include "gba_platform_soundcontext.hpp"


SoundContext snd_ctx;


static const struct AudioTrack {
    const char* name_;
    const AudioSample* data_;
    int length_; // NOTE: For music, this is the track length in 32 bit words,
                 // but for sounds, length_ reprepresents bytes.
} music_tracks[] = {};


static const AudioTrack* find_music(const char* name)
{
    for (auto& track : music_tracks) {

        if (str_cmp(name, track.name_) == 0) {
            return &track;
        }
    }

    return nullptr;
}


// NOTE: Between remixing the audio track down to 8-bit 16kHz signed, generating
// assembly output, adding the file to CMake, adding the include, and adding the
// sound to the sounds array, it's just too tedious to keep working this way...
#include "data/sound_msg.hpp"


static const AudioTrack sounds[] = {DEF_SOUND(msg, sound_msg)};


static const AudioTrack* get_sound(const char* name)
{
    for (auto& sound : sounds) {
        if (str_cmp(name, sound.name_) == 0) {
            return &sound;
        }
    }
    return nullptr;
}


Microseconds Platform::Speaker::track_length(const char* name)
{
    if (const auto music = find_music(name)) {
        return (music->length_ * (sizeof(u32))) / 0.016f;
    }

    if (const auto sound = get_sound(name)) {
        return sound->length_ / 0.016f;
    }

    return 0;
}


static std::optional<ActiveSoundInfo> make_sound(const char* name)
{
    if (auto sound = get_sound(name)) {
        return ActiveSoundInfo{0, sound->length_, sound->data_, 0};
    } else {
        return {};
    }
}


// If you're going to edit any of the variables used by the interrupt handler
// for audio playback, you should use this helper function.
template <typename F> auto modify_audio(F&& callback)
{
    irqDisable(IRQ_TIMER0);
    callback();
    irqEnable(IRQ_TIMER0);
}


bool Platform::Speaker::is_sound_playing(const char* name)
{
    if (auto sound = get_sound(name)) {
        bool playing = false;
        modify_audio([&] {
            for (const auto& info : snd_ctx.active_sounds) {
                if ((s8*)sound->data_ == info.data_) {
                    playing = true;
                    return;
                }
            }
        });

        return playing;
    }
    return false;
}


void Platform::Speaker::set_position(const Vec2<Float>&)
{
    // We don't support spatialized audio on the gameboy.
}


static void push_sound(const ActiveSoundInfo& info)
{
    modify_audio([&] {
        if (not snd_ctx.active_sounds.full()) {
            snd_ctx.active_sounds.push_back(info);

        } else {
            ActiveSoundInfo* lowest = snd_ctx.active_sounds.begin();
            for (auto it = snd_ctx.active_sounds.begin();
                 it not_eq snd_ctx.active_sounds.end();
                 ++it) {
                if (it->priority_ < lowest->priority_) {
                    lowest = it;
                }
            }

            if (lowest not_eq snd_ctx.active_sounds.end() and
                lowest->priority_ < info.priority_) {
                snd_ctx.active_sounds.erase(lowest);
                snd_ctx.active_sounds.push_back(info);
            }
        }
    });
}


void Platform::Speaker::play_sound(const char* name,
                                   int priority,
                                   std::optional<Vec2<Float>> position)
{
    (void)position; // We're not using position data, because on the gameboy
                    // advance, we aren't supporting spatial audio.

    if (auto info = make_sound(name)) {
        info->priority_ = priority;
        push_sound(*info);
        return;
    }

    auto sound_file = platform->fs().get_file(name);
    if (sound_file.data_) {
        ActiveSoundInfo info{0, static_cast<s32>(sound_file.size_), 0, 0};
        info.priority_ = priority;
        info.data_ = reinterpret_cast<const s8*>(sound_file.data_);
        push_sound(info);
    }
}


static void clear_music()
{
    // The audio interrupt handler can be smaller and simpler if we use a track
    // of empty bytes to represent scenarios where music is not playing, rather
    // than adding another if condition to the audio isr.
    snd_ctx.music_track = reinterpret_cast<const AudioSample*>(null_music);
    snd_ctx.music_track_length = null_music_len - 1;
    snd_ctx.music_track_pos = 0;
}


static void stop_music()
{
    modify_audio([] { clear_music(); });
}


void Platform::Speaker::stop_music()
{
    ::stop_music();
}


static void play_music(const char* name, Microseconds offset)
{
    auto music_file = platform->fs().get_file(name);
    if (music_file.data_ == nullptr) {
        warning(*::platform, "failed to find music file!");
        return;
    }

    const Microseconds sample_offset = offset * 0.016f; // NOTE: because 16kHz

    modify_audio([&] {
        snd_ctx.music_track_length = music_file.size_;
        snd_ctx.music_track = reinterpret_cast<const s8*>(music_file.data_);
        snd_ctx.music_track_pos = sample_offset % music_file.size_;
    });
}


void Platform::Speaker::play_music(const char* name, Microseconds offset)
{
    // NOTE: The sound sample needs to be mono, and 8-bit signed. To export this
    // format from Audacity, convert the tracks to mono via the Tracks dropdown,
    // and then export as raw, in the format 8-bit signed.
    //
    // Also, important to convert the sound file frequency to 16kHz.

    this->stop_music();

    ::play_music(name, offset);

    // FIXME!!!!!! Mysteriously, there's a weird audio glitch, where the sound
    // effects, but not the music, get all glitched out until two sounds are
    // played consecutively. I've spent hours trying to figure out what's going
    // wrong, and I haven't solved this one yet, so for now, just play a couple
    // quiet sounds. To add further confusion, after adjusting the instruction
    // prefetch and waitstats, I need to play three sounds
    // consecutively... obviously my interrupt service routine for the audio is
    // flawed somehow. Do I need to completely disable the timers and sound
    // chip, as well as the audio interrupts, when playing new sounds? Does
    // disabling the audio interrupts when queueing a new sound effect cause
    // audio artifacts, because the sound chip is not receiving samples?
    play_sound("msg", 0);
    play_sound("msg", 0);
    play_sound("msg", 0);
}


Platform::Speaker::Speaker()
{
}


////////////////////////////////////////////////////////////////////////////////
// Misc
////////////////////////////////////////////////////////////////////////////////


#define REG_SGFIFOA *(volatile u32*)0x40000A0


// NOTE: I tried to move this audio update interrupt handler to IWRAM, but the
// sound output became sort of glitchy, and I noticed some tearing in the
// display. At the same time, the game was less laggy, so maybe when I work out
// the kinks, this function will eventually be moved to arm code instead of
// thumb.
//
// NOTE: We play music at 16kHz, and we can load four samples upon each audio
// interrupt, i.e. 4000 interrupts per second, i.e. approximately sixty-seven
// interrupts per frame (given sixty fps). Considering how many interrupts we're
// dealing with here, this isr should be kept small and simple. We're only
// supporting one music channel (which loops by default), and three concurrent
// sound channels, in our audio mixer.
//
// Considering the number of interrupts that we're dealing with here, one might
// wonder why we aren't using one of the DMA channels to load sound samples. The
// DMA halts the CPU, which could result in missed serial I/O interrupts during
// multiplayer games.
static void audio_update_isr()
{
    alignas(4) AudioSample mixing_buffer[4];

    for (int i = 0; i < 4; ++i) {
        mixing_buffer[i] = snd_ctx.music_track[snd_ctx.music_track_pos++];
    }

    if (UNLIKELY(snd_ctx.music_track_pos > snd_ctx.music_track_length)) {
        snd_ctx.music_track_pos = 0;
    }

    // Maybe the world's worst sound mixing code...
    for (auto it = snd_ctx.active_sounds.begin();
         it not_eq snd_ctx.active_sounds.end();) {
        if (UNLIKELY(it->position_ + 4 >= it->length_)) {
            it = snd_ctx.active_sounds.erase(it);

        } else {
            for (int i = 0; i < 4; ++i) {
                mixing_buffer[i] += it->data_[it->position_++];
            }

            ++it;
        }
    }

    REG_SGFIFOA = *((u32*)mixing_buffer);
}


void Platform::soft_exit()
{
    Stop();
}


static Microseconds watchdog_counter;


static std::optional<Platform::WatchdogCallback> watchdog_callback;



static void vblank_isr()
{
    const auto ten_seconds = 600; // approx. 60 fps
    watchdog_counter += 1;

    if (UNLIKELY(::watchdog_counter > ten_seconds)) {
        if (::platform and ::watchdog_callback) {
            (*::watchdog_callback)(*platform);
        }

        restart();
    }

    ++vblank_count;
}


void Platform::feed_watchdog()
{
    ::watchdog_counter = 0;
}


void Platform::on_watchdog_timeout(WatchdogCallback callback)
{
    ::watchdog_callback.emplace(callback);
}


__attribute__((section(".iwram"), long_call)) void
cartridge_interrupt_handler();


bool use_optimized_waitstates = false;


// EWRAM is large, but has a narrower bus. The platform offers a window into
// EWRAM, called scratch space, for non-essential stuff. Right now, I am setting
// the buffer to ~100K in size. One could theoretically make the buffer almost
// 256kB, because I am using none of EWRAM as far as I know...
static EWRAM_DATA
    ObjectPool<RcBase<Platform::ScratchBuffer,
                      Platform::scratch_buffer_count>::ControlBlock,
               Platform::scratch_buffer_count>
        scratch_buffer_pool;


static int scratch_buffers_in_use = 0;
static int scratch_buffer_highwater = 0;


Platform::ScratchBufferPtr Platform::make_scratch_buffer()
{
    auto finalizer = [](RcBase<Platform::ScratchBuffer,
                               scratch_buffer_count>::ControlBlock* ctrl) {
        --scratch_buffers_in_use;
        ctrl->pool_->post(ctrl);
    };

    auto maybe_buffer = Rc<ScratchBuffer, scratch_buffer_count>::create(
        &scratch_buffer_pool, finalizer);
    if (maybe_buffer) {
        ++scratch_buffers_in_use;
        if (scratch_buffers_in_use > scratch_buffer_highwater) {
            scratch_buffer_highwater = scratch_buffers_in_use;

            StringBuffer<60> str = "sbr highwater: ";
            char buf[10];
            english__to_string(scratch_buffer_highwater, buf, 10);

            str += buf;

            info(*::platform, str.c_str());
        }
        return *maybe_buffer;
    } else {
        screen().fade(1.f, ColorConstant::electric_blue);
        error(*this, "scratch buffer pool exhausted");
        fatal();
    }
}


int Platform::scratch_buffers_remaining()
{
    return scratch_buffer_count - scratch_buffers_in_use;
}


Platform::~Platform()
{
    // ...
}


struct GlyphMapping {
    utf8::Codepoint character_;

    // -1 represents unassigned. Mapping a tile into memory sets the reference
    //  count to zero. When a call to Platform::set_tile reduces the reference
    //  count back to zero, the tile is once again considered to be unassigned,
    //  and will be set to -1.
    s16 reference_count_ = -1;

    bool valid() const
    {
        return reference_count_ > -1;
    }
};


static constexpr const auto glyph_start_offset = 1;
static constexpr const auto glyph_mapping_count = 78;

struct GlyphTable {
    GlyphMapping mappings_[glyph_mapping_count];
};

static std::optional<DynamicMemory<GlyphTable>> glyph_table;


static void audio_start()
{
    clear_music();

    REG_SOUNDCNT_H =
        0x0B0F; //DirectSound A + fifo reset + max volume to L and R
    REG_SOUNDCNT_X = 0x0080; //turn sound chip on

    irqEnable(IRQ_TIMER1);
    irqSet(IRQ_TIMER1, audio_update_isr);

    REG_TM0CNT_L = 0xffff;
    REG_TM1CNT_L = 0xffff - 3; // I think that this is correct, but I'm not
                               // certain... so we want to play four samples at
                               // a time, which means that by subtracting three
                               // from the initial count, the timer will
                               // overflow at the correct rate, right?

    // While it may look like TM0 is unused, it is in fact used for setting the
    // sample rate for the digital audio chip.
    REG_TM0CNT_H = 0x0083;
    REG_TM1CNT_H = 0x00C3;
}


// We want our code to be resiliant to cartridges lacking an RTC chip. Run the
// timer-based delta clock for a while, and make sure that the RTC also counted
// up.
static bool rtc_verify_operability(Platform& pfrm)
{
    return true;
    Microseconds counter = 0;

    const auto tm1 = pfrm.system_clock().now();

    while (counter < seconds(1) + milliseconds(250)) {
        counter += pfrm.delta_clock().reset();
    }

    const auto tm2 = pfrm.system_clock().now();

    return tm1 and tm2 and time_diff(*tm1, *tm2) > 0;
}


static bool rtc_faulty = false;


static std::optional<DateTime> start_time;


std::optional<DateTime> Platform::startup_time() const
{
    return ::start_time;
}


Platform::Platform()
{
    // Not sure how else to determine whether the cartridge has sram, flash, or
    // something else. An sram write will fail if the cartridge ram is flash, so
    // attempt to save, and if the save fails, assume flash. I don't really know
    // anything about the EEPROM hardware interface...
    static const int sram_test_const = 0xAAAAAAAA;
    sram_save(&sram_test_const, log_write_loc, sizeof sram_test_const);

    int sram_test_result = 0;
    sram_load(&sram_test_result, log_write_loc, sizeof sram_test_result);

    if (sram_test_result not_eq sram_test_const) {
        save_using_flash = true;
        info(*this, "SRAM write failed, falling back to FLASH");
    }

    glyph_table.emplace(allocate_dynamic<GlyphTable>(*this));
    if (not glyph_table) {
        error(*this, "failed to allocate glyph table");
        fatal();
    }

    // IMPORTANT: No calls to map_glyph() are allowed before reaching this
    // line. Otherwise, the glyph table has not yet been constructed.

    info(*this, "Verifying BIOS...");

    switch (BiosCheckSum()) {
    case BiosVersion::NDS:
        info(*this, "BIOS matches Nintendo DS");
        break;
    case BiosVersion::GBA:
        info(*this, "BIOS matches GAMEBOY Advance");
        break;
    default:
        warning(*this, "BIOS checksum failed, may be corrupt");
        break;
    }

    // NOTE: Non-sequential 8 and sequential 3 seem to work well for Cart 0 wait
    // states, although setting these options unmasks a few obscure audio bugs,
    // the game displays visibly less tearing. The cartridge prefetch unmasks
    // even more aggressive audio bugs, and doesn't seem to grant obvious
    // performance benefits, so I'm leaving the cartridge prefetch turned off...
    if (use_optimized_waitstates) {
        // Although there is less tearing when running with optimized
        // waitstates, I actually prefer the feature turned off. I really tuned
        // the feel of the controls before I knew about waitstates, and
        // something just feels off to me when turning this feature on. The game
        // is almost too smooth.
        REG_WAITCNT = 0b0000001100010111;
        info(*this, "enabled optimized waitstates...");
    }

    // NOTE: initializing the system clock is easier before interrupts are
    // enabled, because the system clock pulls data from the gpio port on the
    // cartridge.
    system_clock_.init(*this);

    irqInit();
    irqEnable(IRQ_VBLANK);

    irqEnable(IRQ_KEYPAD);
    key_wake_isr();

    irqSet(IRQ_TIMER3, [] {
        delta_total += 0xffff;

        REG_TM3CNT_H = 0;
        REG_TM3CNT_L = 0;
        REG_TM3CNT_H = 1 << 7 | 1 << 6;
    });

    if ((rtc_faulty = not rtc_verify_operability(*this))) {
        info(*this, "RTC chip appears either non-existant or non-functional");
    } else {
        ::start_time = system_clock_.now();

        StringBuffer<100> str = "startup time: ";

        log_format_time(str, *::start_time);

        info(*::platform, str.c_str());
    }

    // Surprisingly, the default value of SIOCNT is not necessarily zero! The
    // source of many past serial comms headaches...
    REG_SIOCNT = 0;


    fill_overlay(0);

    audio_start();

    irqSet(IRQ_VBLANK, vblank_isr);

    irqEnable(IRQ_GAMEPAK);
    irqSet(IRQ_GAMEPAK, cartridge_interrupt_handler);

    for (u32 i = 0; i < Screen::sprite_limit; ++i) {
        // This was a really insidious bug to track down! When failing to hide
        // unused attributes in the back buffer, the uninitialized objects punch
        // a 1 tile (8x8 pixel) hole in the top left corner of the overlay
        // layer, but not exactly. The tile in the high priority background
        // layer still shows up, but lower priority sprites show through the top
        // left tile, I guess I'm observing some weird interaction involving an
        // overlap between a priority 0 tile and a priority 1 sprite: when a
        // priority 1 sprite is sandwitched in between the two tile layers, the
        // priority 0 background tiles seems to be drawn behind the priority 1
        // sprite. I have no idea why!
        object_attribute_back_buffer[i].attribute_2 = ATTR2_PRIORITY(3);
        object_attribute_back_buffer[i].attribute_0 |= attr0_mask::disabled;
    }


    for (auto& prefix : overlay_textures) {
        if (str_cmp(prefix.name_, "overlay") == 0) {
            memcpy16((void*)&MEM_SCREENBLOCKS[sbb_overlay_texture][0],
                     prefix.tile_data_,
                     prefix.tile_data_length_ / 2);

            for (int i = 0; i < 16; ++i) {
                auto from = Color::from_bgr_hex_555(prefix.palette_data_[i]);
                if (not overlay_was_faded) {
                    MEM_BG_PALETTE[16 + i] = from.bgr_hex_555();
                }
            }

            break;
        }
    }
}


static bool glyph_mode = false;


void Platform::enable_glyph_mode(bool enabled)
{
    if (enabled) {
        for (auto& gm : ::glyph_table->obj_->mappings_) {
            gm.reference_count_ = -1;
        }
    }
    glyph_mode = enabled;
}


static u16 overlay_source_pal[16];
static TextureData overlay_file_data;


static std::optional<Platform::FailureReason>
push_overlay_texture(const TextureData& info)
{
    current_overlay_texture = &info;

    init_palette(current_overlay_texture, overlay_palette, true);

    for (int i = 0; i < 16; ++i) {
        auto from = Color::from_bgr_hex_555(overlay_palette[i]);
        if (not overlay_was_faded) {
            MEM_BG_PALETTE[16 + i] = from.bgr_hex_555();
        } else {
            const auto c = nightmode_adjust(real_color(last_color));
            MEM_BG_PALETTE[16 + i] = blend(from, c, last_fade_amt);
        }
    }

    // For the purposes of displaying text, we copy a fixed image into the first
    // eighty-two indices, followed by the user's image.
    for (auto& prefix : overlay_textures) {
        if (str_cmp(prefix.name_, "overlay") == 0) {

            auto consume = info.tile_data_length_ + prefix.tile_data_length_;
            auto exceeded_bytes =
                validate_overlay_texture_size(*platform, consume);

            if (not exceeded_bytes) {
                memcpy16((char*)&MEM_SCREENBLOCKS[sbb_overlay_texture][0] +
                             prefix.tile_data_length_,
                         info.tile_data_,
                         info.tile_data_length_ / 2);

                return {};
            } else {
                Platform::FailureReason r;
                r += "exceeded overlay vram capacity by ";

                char buffer[32];
                english__to_string(exceeded_bytes / 32, buffer, 10);

                r += buffer;
                r += " tile(s).";

                return r;
            }
        }
    }

    return {};
}


std::optional<Platform::FailureReason>
Platform::load_overlay_texture(const char* name, int addr, int len)
{
    StringBuffer<48> palette_file(name);
    palette_file += ".pal";

    const auto img = addr ? fs().get_file(addr, len) : fs().get_file(name);
    const auto palette = addr ? fs().next_file(addr, len) : fs().get_file(palette_file.c_str());

    if (img.data_ and palette.data_) {

        memcpy(overlay_source_pal, palette.data_, sizeof overlay_source_pal);

        TextureData& info = overlay_file_data;
        info.name_ = name;
        info.tile_data_ = (const unsigned int*)img.data_;
        info.palette_data_ = overlay_source_pal;
        info.tile_data_length_ = img.size_;
        info.palette_data_length_ = 16;

        return push_overlay_texture(info);
    }

    for (auto& info : overlay_textures) {

        if (str_cmp(name, info.name_) == 0) {

            return push_overlay_texture(info);
        }
    }


    FailureReason r;
    r += "missing ";
    r += name;
    r += " or ";
    r += palette_file;
    r += ".";

    return r;
}


static const TileDesc bad_glyph = 82;


static constexpr int vram_tile_size()
{
    // 8 x 8 x (4 bitsperpixel / 8 bitsperbyte)
    return 32;
}


// Rather than doing tons of extra work to keep the palettes
// coordinated between different image files, use tile index
// 81 as a registration block, which holds a set of colors
// to use when mapping glyphs into vram.
static u8* font_index_tile()
{
    return (u8*)&MEM_SCREENBLOCKS[sbb_overlay_texture][0] +
           ((81) * vram_tile_size());
}


struct FontColorIndices {
    int fg_;
    int bg_;
};


static FontColorIndices font_color_indices()
{
    const auto index = font_index_tile();
    return {index[0] & 0x0f, index[1] & 0x0f};
}


// This code uses a lot of naive algorithms for searching the glyph mapping
// table, potentially could be sped up, but we don't want to use any extra
// memory, we've only got 256K to work with, and the table is big enough as it
// is.
TileDesc Platform::map_glyph(const utf8::Codepoint& glyph,
                             TextureCpMapper mapper)
{
    if (not glyph_mode) {
        return bad_glyph;
    }

    for (TileDesc tile = 0; tile < glyph_mapping_count; ++tile) {
        auto& gm = ::glyph_table->obj_->mappings_[tile];
        if (gm.valid() and gm.character_ == glyph) {
            return glyph_start_offset + tile;
        }
    }

    const auto mapping_info = mapper(glyph);

    if (not mapping_info) {
        return bad_glyph;
    }

    // NOTE: The linker or some other part of the toolchain seems to have
    // trouble with large image file sizes, because when I try to include a
    // single gigantic charset file in the project, I end up with garbage
    // data. I believe that I've narrowed it down to some part of the build
    // toolchain, because when I simply use a truncated charset file, everything
    // works fine. A really bizzare issue.

    const int binsize = (12000 / 8);
    const int bin = mapping_info->offset_ / (12000 / 8);
    int adjusted_offset = mapping_info->offset_;
    if (bin > 0) {
        adjusted_offset -= binsize * bin;
        adjusted_offset += 1 * bin; // +1 for the font index tile. FIXME: in
                                    // future versions, the script that
                                    // generates font tile mappings should be
                                    // responsible for adding a +1 offset for
                                    // each charset bin.
    }

    char buf[15];

    StringBuffer<100> charset_name = "charset";
    if (bin == 0) {
        charset_name = "charset0";
    } else {
        english__to_string(bin, buf, 10);
        charset_name += buf;
    }


    for (auto& info : overlay_textures) {
        if (str_cmp(charset_name.c_str(), info.name_) == 0) {
            for (TileDesc t = 0; t < glyph_mapping_count; ++t) {
                auto& gm = ::glyph_table->obj_->mappings_[t];
                if (not gm.valid()) {
                    gm.character_ = glyph;
                    gm.reference_count_ = 0;

                    // 8 x 8 x (4 bitsperpixel / 8 bitsperbyte)
                    constexpr int tile_size = vram_tile_size();

                    // u8 buffer[tile_size] = {0};
                    // memcpy16(buffer,
                    //          (u8*)&MEM_SCREENBLOCKS[sbb_overlay_texture][0] +
                    //              ((81) * tile_size),
                    //          tile_size / 2);

                    const auto colors = font_color_indices();

                    // We need to know which color to use as the background
                    // color, and which color to use as the foreground
                    // color. Each charset needs to store a reference pixel in
                    // the top left corner, representing the background color,
                    // otherwise, we have no way of knowing which pixel color to
                    // substitute where!
                    const auto bg_color = ((u8*)info.tile_data_)[0] & 0x0f;

                    u8 buffer[tile_size] = {0};


                    auto k_src =
                        info.tile_data_ + (adjusted_offset * tile_size) /
                                              sizeof(decltype(info.tile_data_));

                    memcpy16(buffer, k_src, tile_size / 2);

                    for (int i = 0; i < tile_size; ++i) {
                        auto c = buffer[i];
                        if (c & bg_color) {
                            buffer[i] = colors.bg_;
                        } else {
                            buffer[i] = colors.fg_;
                        }
                        if (c & (bg_color << 4)) {
                            buffer[i] |= colors.bg_ << 4;
                        } else {
                            buffer[i] |= colors.fg_ << 4;
                        }
                    }

                    // FIXME: Why do these magic constants work? I wish better
                    // documentation existed for how the gba tile memory worked.
                    // I thought, that the tile size would be 32, because we
                    // have 4 bits per pixel, and 8x8 pixel tiles. But the
                    // actual number of bytes in a tile seems to be half of the
                    // expected number. Also, in vram, it seems like the tiles
                    // do seem to be 32 bytes apart after all...
                    memcpy16((u8*)&MEM_SCREENBLOCKS[sbb_overlay_texture][0] +
                                 ((t + glyph_start_offset) * tile_size),
                             buffer,
                             tile_size / 2);

                    return t + glyph_start_offset;
                }
            }
        }
    }
    return bad_glyph;
}


static bool is_glyph(u16 t)
{
    return t >= glyph_start_offset and
           t - glyph_start_offset < glyph_mapping_count;
}


void Platform::fill_overlay(u16 tile)
{
    if (glyph_mode and is_glyph(tile)) {
        // This is moderately complicated to implement, better off just not
        // allowing fills for character tiles.
        return;
    }

    const u16 tile_info = tile | SE_PALBANK(1);
    const u32 fill_word = tile_info | (tile_info << 16);

    u32* const mem = (u32*)overlay_back_buffer;
    overlay_back_buffer_changed = true;

    for (unsigned i = 0; i < sizeof(ScreenBlock) / (sizeof(u32)); ++i) {
        mem[i] = fill_word;
    }

    if (glyph_mode) {
        for (auto& gm : ::glyph_table->obj_->mappings_) {
            gm.reference_count_ = -1;
        }
    }
}


static void set_overlay_tile(Platform& pfrm, u16 x, u16 y, u16 val, int palette)
{
    if (glyph_mode) {
        // This is where we handle the reference count for mapped glyphs. If
        // we are overwriting a glyph with different tile, then we can
        // decrement a glyph's reference count. Then, we want to increment
        // the incoming glyph's reference count if the incoming tile is
        // within the range of the glyph table.

        const auto old_tile = pfrm.get_tile(Layer::overlay, x, y);
        if (old_tile not_eq val) {
            if (is_glyph(old_tile)) {
                auto& gm = ::glyph_table->obj_
                               ->mappings_[old_tile - glyph_start_offset];
                if (gm.valid()) {
                    gm.reference_count_ -= 1;

                    if (gm.reference_count_ == 0) {
                        gm.reference_count_ = -1;
                        gm.character_ = 0;
                    }
                } else {
                    error(pfrm,
                          "existing tile is a glyph, but has no"
                          " mapping table entry!");
                }
            }

            if (is_glyph(val)) {
                auto& gm =
                    ::glyph_table->obj_->mappings_[val - glyph_start_offset];
                if (not gm.valid()) {
                    // Not clear exactly what to do here... Somehow we've
                    // gotten into an erroneous state, but not a permanently
                    // unrecoverable state (tile isn't valid, so it'll be
                    // overwritten upon the next call to map_tile).
                    warning(pfrm, "invalid assignment to glyph table");
                    return;
                }
                gm.reference_count_++;
            }
        }
    }

    overlay_back_buffer[x + y * 32] = val | SE_PALBANK(palette);
    overlay_back_buffer_changed = true;
}


// Now for custom-colored text... we're using three of the background palettes
// already, one for the map_0 layer (shared with the background layer), one for
// the map_1 layer, and one for the overlay. That leaves 13 remaining palettes,
// which we can use for colored text. But we may not want to use up all of the
// available extra palettes, so let's just allocate four of them toward custom
// text colors for now...
static const PaletteBank custom_text_palette_begin = 3;
static const PaletteBank custom_text_palette_end = 7;
static const auto custom_text_palette_count =
    custom_text_palette_end - custom_text_palette_begin;

static PaletteBank custom_text_palette_write_ptr = custom_text_palette_begin;
// static const TextureData* custom_text_palette_source_texture = nullptr;


void Platform::set_tile(u16 x, u16 y, TileDesc glyph, const FontColors& colors)
{
    if (not glyph_mode or not is_glyph(glyph)) {
        return;
    }

    // If the current overlay texture changed, then we'll need to clear out any
    // palettes that we've constructed. The indices of the glyph binding sites
    // in the palette bank may have changed when we loaded a new texture.
    // FIXME!!!
    // if (custom_text_palette_source_texture and
    //     custom_text_palette_source_texture not_eq current_overlay_texture) {

    //     for (auto p = custom_text_palette_begin; p < custom_text_palette_end;
    //          ++p) {
    //         for (int i = 0; i < 16; ++i) {
    //             MEM_BG_PALETTE[p * 16 + i] = 0;
    //         }
    //     }

    //     custom_text_palette_source_texture = current_overlay_texture;
    // }

    const auto default_colors = font_color_indices();

    const auto fg_color_hash =
        nightmode_adjust(real_color(colors.foreground_)).bgr_hex_555();

    const auto bg_color_hash =
        nightmode_adjust(real_color(colors.background_)).bgr_hex_555();

    auto existing_mapping = [&]() -> std::optional<PaletteBank> {
        for (auto i = custom_text_palette_begin; i < custom_text_palette_end;
             ++i) {
            if (MEM_BG_PALETTE[i * 16 + default_colors.fg_] == fg_color_hash and
                MEM_BG_PALETTE[i * 16 + default_colors.bg_] == bg_color_hash) {

                return i;
            }
        }
        return {};
    }();

    if (existing_mapping) {
        set_overlay_tile(*this, x, y, glyph, *existing_mapping);
    } else {
        const auto target = custom_text_palette_write_ptr;

        MEM_BG_PALETTE[target * 16 + default_colors.fg_] = fg_color_hash;
        MEM_BG_PALETTE[target * 16 + default_colors.bg_] = bg_color_hash;

        set_overlay_tile(*this, x, y, glyph, target);

        custom_text_palette_write_ptr =
            ((target + 1) - custom_text_palette_begin) %
                custom_text_palette_count +
            custom_text_palette_begin;

        if (custom_text_palette_write_ptr == custom_text_palette_begin) {
            warning(*this, "wraparound in custom text palette alloc");
        }
    }
}


void Platform::set_tile(Layer layer, u16 x, u16 y, u16 val)
{
    switch (layer) {
    case Layer::overlay:
        if (x > 31 or y > 31) {
            return;
        }
        set_overlay_tile(*this, x, y, val, 1);
        break;

    case Layer::map_1:
        if (x > 63 or y > 63) {
            return;
        }
        if (x < 32 and y < 32) {
            MEM_SCREENBLOCKS[sbb_t1_tiles][x + y * 32] = val | SE_PALBANK(2);
        } else if (y < 32) {
            MEM_SCREENBLOCKS[sbb_t1_tiles + 1][(x - 32) + y * 32] =
                val | SE_PALBANK(2);
        } else if (x < 32) {
            MEM_SCREENBLOCKS[sbb_t1_tiles + 2][x + (y - 32) * 32] =
                val | SE_PALBANK(2);
        } else {
            MEM_SCREENBLOCKS[sbb_t1_tiles + 3][(x - 32) + (y - 32) * 32] =
                val | SE_PALBANK(2);
        }
        break;

    case Layer::map_0:
        if (x > 63 or y > 63) {
            return;
        }
        if (x < 32 and y < 32) {
            MEM_SCREENBLOCKS[sbb_t0_tiles][x + y * 32] = val;
        } else if (y < 32) {
            MEM_SCREENBLOCKS[sbb_t0_tiles + 1][(x - 32) + y * 32] = val;
        } else if (x < 32) {
            MEM_SCREENBLOCKS[sbb_t0_tiles + 2][x + (y - 32) * 32] = val;
        } else {
            MEM_SCREENBLOCKS[sbb_t0_tiles + 3][(x - 32) + (y - 32) * 32] = val;
        }
        break;

    case Layer::background:
        if (x > 31 or y > 31) {
            return;
        }
        MEM_SCREENBLOCKS[sbb_bg_tiles][x + y * 32] = val;
        break;
    }
}


////////////////////////////////////////////////////////////////////////////////
// NetworkPeer
////////////////////////////////////////////////////////////////////////////////


Platform::NetworkPeer::NetworkPeer()
{
}


static int multiplayer_is_master()
{
    return (REG_SIOCNT & (1 << 2)) == 0 and (REG_SIOCNT & (1 << 3));
}


// NOTE: you may only call this function immediately after a transmission,
// otherwise, may return a garbage value.
static int multiplayer_error()
{
    return REG_SIOCNT & (1 << 6);
}


// NOTE: This function should only be called in a serial interrupt handler,
// otherwise, may return a garbage value.


static bool multiplayer_validate_modes()
{
    // 1 if all GBAs are in the correct mode, 0 otherwise.
    return REG_SIOCNT & (1 << 3);
}


static bool multiplayer_validate()
{
    if (not multiplayer_validate_modes()) {
        return false;
    } else {
    }
    return true;
}


// The gameboy Multi link protocol always sends data, no matter what, even if we
// do not have any new data to put in the send buffer. Because there is no
// distinction between real data and empty transmits, we will transmit in
// fixed-size chunks. The receiver knows when it's received a whole message,
// after a specific number of iterations. Now, there are other ways, potentially
// better ways, to handle this situation. But this way seems easiest, although
// probably uses a lot of unnecessary bandwidth. Another drawback: the poller
// needs ignore messages that are all zeroes. Accomplished easily enough by
// prefixing the sent message with an enum, where the zeroth enumeration is
// unused.
static const int message_iters =
    Platform::NetworkPeer::max_message_size / sizeof(u16);


struct WireMessage {
    u16 data_[message_iters] = {};
};


using TxInfo = WireMessage;
using RxInfo = WireMessage;


static bool multiplayer_connected;


struct MultiplayerComms {
    int rx_loss = 0;
    int tx_loss = 0;

    int rx_message_count = 0;
    int tx_message_count = 0;


    static constexpr const int tx_ring_size = 32;
    ObjectPool<TxInfo, tx_ring_size> tx_message_pool;

    int tx_ring_write_pos = 0;
    int tx_ring_read_pos = 0;
    TxInfo* tx_ring[tx_ring_size] = {nullptr};


    static constexpr const int rx_ring_size = 64;
    ObjectPool<RxInfo, rx_ring_size> rx_message_pool;

    int rx_ring_write_pos = 0;
    int rx_ring_read_pos = 0;
    RxInfo* rx_ring[rx_ring_size] = {nullptr};

    int rx_iter_state = 0;
    RxInfo* rx_current_message =
        nullptr; // Note: we will drop the first message, oh well.

    // The multi serial io mode always transmits, even when there's nothing to
    // send. At first, I was allowing zeroed out messages generated by the
    // platform to pass through to the user. But doing so takes up a lot of
    // space in the rx buffer, so despite the inconvenience, for performance
    // reasons, I am going to have to require that messages containing all
    // zeroes never be sent by the user.
    bool rx_current_all_zeroes = true;

    int transmit_busy_count = 0;


    int tx_iter_state = 0;
    TxInfo* tx_current_message = nullptr;

    int null_bytes_written = 0;

    bool is_host = false;

    RxInfo* poller_current_message = nullptr;
};


static MultiplayerComms multiplayer_comms;


static TxInfo* tx_ring_pop()
{
    auto& mc = multiplayer_comms;


    TxInfo* msg = nullptr;

    for (int i = mc.tx_ring_read_pos; i < mc.tx_ring_read_pos + mc.tx_ring_size;
         ++i) {
        auto index = i % mc.tx_ring_size;
        if (mc.tx_ring[index]) {
            msg = mc.tx_ring[index];
            mc.tx_ring[index] = nullptr;
            mc.tx_ring_read_pos = index;
            return msg;
        }
    }

    mc.tx_ring_read_pos += 1;
    mc.tx_ring_read_pos %= mc.tx_ring_size;

    // The transmit ring is completely empty!
    return nullptr;
}


static void rx_ring_push(RxInfo* message)
{
    auto& mc = multiplayer_comms;

    mc.rx_message_count += 1;

    if (mc.rx_ring[mc.rx_ring_write_pos]) {
        // The reader does not seem to be keeping up!
        mc.rx_loss += 1;

        auto lost_message = mc.rx_ring[mc.rx_ring_write_pos];

        mc.rx_ring[mc.rx_ring_write_pos] = nullptr;
        mc.rx_message_pool.post(lost_message);
    }

    mc.rx_ring[mc.rx_ring_write_pos] = message;
    mc.rx_ring_write_pos += 1;
    mc.rx_ring_write_pos %= mc.rx_ring_size;
}


static RxInfo* rx_ring_pop()
{
    auto& mc = multiplayer_comms;

    RxInfo* msg = nullptr;

    for (int i = mc.rx_ring_read_pos; i < mc.rx_ring_read_pos + mc.rx_ring_size;
         ++i) {
        auto index = i % mc.rx_ring_size;

        if (mc.rx_ring[index]) {
            msg = mc.rx_ring[index];
            mc.rx_ring[index] = nullptr;
            mc.rx_ring_read_pos = index;

            return msg;
        }
    }

    mc.rx_ring_read_pos += 1;
    mc.rx_ring_read_pos %= mc.rx_ring_size;

    return nullptr;
}


static void multiplayer_rx_receive()
{
    auto& mc = multiplayer_comms;

    if (mc.rx_iter_state == message_iters) {
        if (mc.rx_current_message) {
            if (mc.rx_current_all_zeroes) {
                mc.rx_message_pool.post(mc.rx_current_message);
            } else {
                rx_ring_push(mc.rx_current_message);
            }
        }

        mc.rx_current_all_zeroes = true;

        mc.rx_current_message = mc.rx_message_pool.get();
        if (not mc.rx_current_message) {
            mc.rx_loss += 1;
        }
        mc.rx_iter_state = 0;
    }

    if (mc.rx_current_message) {
        const auto val =
            multiplayer_is_master() ? REG_SIOMULTI1 : REG_SIOMULTI0;
        if (mc.rx_current_all_zeroes and val) {
            mc.rx_current_all_zeroes = false;
        }
        mc.rx_current_message->data_[mc.rx_iter_state++] = val;

    } else {
        mc.rx_iter_state++;
    }
}


static bool multiplayer_busy()
{
    return REG_SIOCNT & SIO_START;
}


bool Platform::NetworkPeer::send_message(const Message& message)
{
    if (message.length_ > sizeof(TxInfo::data_)) {
        ::platform->fatal();
    }

    if (not is_connected()) {
        return false;
    }

    // TODO: uncomment this block if we actually see issues on the real hardware...
    // if (tx_iter_state == message_iters) {
    //     // Decreases the likelihood of manipulating data shared by the interrupt
    //     // handlers. See related comment in the poll_message() function.
    //     return false;
    // }

    auto& mc = multiplayer_comms;


    if (mc.tx_ring[mc.tx_ring_write_pos]) {
        // The writer does not seem to be keeping up! Guess we'll have to drop a
        // message :(
        mc.tx_loss += 1;

        auto lost_message = mc.tx_ring[mc.tx_ring_write_pos];
        mc.tx_ring[mc.tx_ring_write_pos] = nullptr;

        mc.tx_message_pool.post(lost_message);
    }

    auto msg = mc.tx_message_pool.get();
    if (not msg) {
        // error! Could not transmit messages fast enough, i.e. we've exhausted
        // the message pool! How to handle this condition!?
        mc.tx_loss += 1;
        return false;
    }

    __builtin_memcpy(msg->data_, message.data_, message.length_);

    mc.tx_ring[mc.tx_ring_write_pos] = msg;
    mc.tx_ring_write_pos += 1;
    mc.tx_ring_write_pos %= mc.tx_ring_size;

    return true;
}


static void multiplayer_tx_send()
{
    auto& mc = multiplayer_comms;

    if (mc.tx_iter_state == message_iters) {
        if (mc.tx_current_message) {
            mc.tx_message_pool.post(mc.tx_current_message);
            mc.tx_message_count += 1;
        }
        mc.tx_current_message = tx_ring_pop();
        mc.tx_iter_state = 0;
    }

    if (mc.tx_current_message) {
        REG_SIOMLT_SEND = mc.tx_current_message->data_[mc.tx_iter_state++];
    } else {
        mc.null_bytes_written += 2;
        mc.tx_iter_state++;
        REG_SIOMLT_SEND = 0;
    }
}


// We want to wait long enough for the minions to prepare TX data for the
// master.
static void multiplayer_schedule_master_tx()
{
    REG_TM2CNT_H = 0x00C1;
    REG_TM2CNT_L = 65000; // Be careful with this delay! Due to manufacturing
                          // differences between Gameboy Advance units, you
                          // really don't want to get too smart, and try to
                          // calculate the time right up to the boundary of
                          // where you expect the interrupt to happen. Allow
                          // some extra wiggle room, for other devices that may
                          // raise a serial receive interrupt later than you
                          // expect. Maybe, this timer could be sped up a bit,
                          // but I don't really know... here's the thing, this
                          // code CURRENTLY WORKS, so don't use a faster timer
                          // interrupt until you've tested the code on a bunch
                          // different real gba units (try out the code on the
                          // original gba, both sp models, etc.)

    irqEnable(IRQ_TIMER2);
    irqSet(IRQ_TIMER2, [] {
        if (multiplayer_busy()) {
            ++multiplayer_comms.transmit_busy_count;
            return; // still busy, try again. The only thing that should kick
                    // off this timer, though, is the serial irq, and the
                    // initial connection, so not sure how we could get into
                    // this state.
        } else {
            irqDisable(IRQ_TIMER2);
            multiplayer_tx_send();
            REG_SIOCNT |= SIO_START;
        }
    });
}


static void multiplayer_schedule_tx()
{
    // If we're the minion, simply enter data into the send queue. The
    // master will wait before initiating another transmit.
    if (multiplayer_is_master()) {
        multiplayer_schedule_master_tx();
    } else {
        multiplayer_tx_send();
    }
}


static void multiplayer_serial_isr()
{
    if (UNLIKELY(multiplayer_error())) {
        ::platform->network_peer().disconnect();
        return;
    }

    multiplayer_comms.is_host = multiplayer_is_master();

    multiplayer_rx_receive();
    multiplayer_schedule_tx();
}


std::optional<Platform::NetworkPeer::Message>
Platform::NetworkPeer::poll_message()
{
    auto& mc = multiplayer_comms;

    if (mc.rx_iter_state == message_iters) {
        // This further decreases the likelihood of messing up the receive
        // interrupt handler by manipulating shared data. We really should be
        // declaring stuff volatile and disabling interrupts, but we cannot
        // easily do those things, for various practical reasons, so we're just
        // hoping that a problematic interrupt during a transmit or a poll is
        // just exceedingly unlikely in practice. The serial interrupt handler
        // runs approximately twice per frame, and the game only transmits a few
        // messages per second. Furthermore, the interrupt handlers only access
        // shared state when rx_iter_state == message_iters, so only one in six
        // interrupts manipulates shared state, i.e. only one occurrence every
        // three or so frames. And for writes to shared data to even be a
        // problem, the interrupt would have to occur between two instructions
        // when writing to the message ring or to the message pool. And on top
        // of all that, we are leaving packets in the rx buffer while
        // rx_iter_state == message iters, so we really shouldn't be writing at
        // the same time anyway. So in practice, the possibility of manipulating
        // shared data is just vanishingly small, although I acknowledge that
        // it's a potential problem. There _IS_ a bug, but I've masked it pretty
        // well (I hope). No issues detectable in an emulator, but we'll see
        // about the real hardware... once my link cable arrives in the mail.
        // P.S.: Tested on actual hardware, works fine.
        return {};
    }
    if (auto msg = rx_ring_pop()) {
        if (UNLIKELY(mc.poller_current_message not_eq nullptr)) {
            // failure to deallocate/consume message!
            mc.rx_message_pool.post(msg);
            disconnect();
            return {};
        }
        mc.poller_current_message = msg;
        return Platform::NetworkPeer::Message{
            reinterpret_cast<byte*>(msg->data_),
            static_cast<int>(sizeof(WireMessage::data_))};
    }
    return {};
}


void Platform::NetworkPeer::poll_consume(u32 size)
{
    auto& mc = multiplayer_comms;

    if (mc.poller_current_message) {
        mc.rx_message_pool.post(mc.poller_current_message);
    } else {
        ::platform->fatal();
    }
    mc.poller_current_message = nullptr;
}


static void __attribute__((noinline)) busy_wait(unsigned max)
{
    for (unsigned i = 0; i < max; i++) {
        __asm__ volatile("" : "+g"(i) : :);
    }
}


static void multiplayer_init(Microseconds timeout)
{
    Microseconds delta = 0;

MASTER_RETRY:
    ::platform->network_peer().disconnect();

    ::platform->sleep(5);

    REG_RCNT = R_MULTI;
    REG_SIOCNT = SIO_MULTI;
    REG_SIOCNT |= SIO_IRQ | SIO_115200;

    irqEnable(IRQ_SERIAL);
    irqSet(IRQ_SERIAL, multiplayer_serial_isr);

    // Put this here for now, not sure whether it's really necessary...
    REG_SIOMLT_SEND = 0x5555;

    while (not multiplayer_validate()) {
        delta += ::platform->delta_clock().reset();
        if (delta > std::max(seconds(3), timeout)) {
            if (not multiplayer_validate_modes()) {
                error(*::platform, "not all GBAs are in MULTI mode");
            }
            ::platform->network_peer().disconnect(); // just for good measure
            REG_SIOCNT = 0;
            irqDisable(IRQ_SERIAL);
            return;
        }
        ::platform->feed_watchdog();
    }

    const char* handshake = "link__v00002";

    if (str_len(handshake) not_eq Platform::NetworkPeer::max_message_size) {
        ::platform->network_peer().disconnect();
        error(*::platform, "handshake string does not equal message size");
        return;
    }

    multiplayer_connected = true;

    ::platform->network_peer().send_message(
        {(byte*)handshake, sizeof handshake});

    multiplayer_schedule_tx();

    while (true) {
        ::platform->feed_watchdog();
        delta += ::platform->delta_clock().reset();
        if (delta > seconds(20)) {
            error(*::platform,
                  "no valid handshake received within a reasonable window");
            ::platform->network_peer().disconnect();
            return;
        } else if (auto msg = ::platform->network_peer().poll_message()) {
            for (u32 i = 0; i < sizeof handshake; ++i) {
                if (((u8*)msg->data_)[i] not_eq handshake[i]) {
                    if (multiplayer_is_master()) {
                        // For the master, if none of the other GBAs are in
                        // multi serial mode yet, the SIOCNT register will show
                        // that all gbas are in a ready state (all of one
                        // device). The master will, therefore, push out a
                        // message, and receive back garbage data. So we want to
                        // keep retrying, in order to account for the scenario
                        // where the other device is not yet plugged in, or the
                        // other player has not initiated their own connection.
                        info(*::platform, "master retrying...");

                        // busy-wait for a bit. This is sort of necessary;
                        // Platform::sleep() does not contribute to the
                        // delta clock offset (by design), so if we don't
                        // burn up some time here, we will take a _long_
                        // time to reach the timeout interval.
                        busy_wait(10000);
                        goto MASTER_RETRY; // lol yikes a goto
                    } else {
                        ::platform->network_peer().disconnect();
                        info(*::platform, "invalid handshake");
                        return;
                    }
                }
            }
            info(*::platform, "validated handshake");
            // irqSet(IRQ_TIMER1, audio_update_fast_isr);
            ::platform->network_peer().poll_consume(sizeof handshake);
            return;
        }
    }
}


void Platform::NetworkPeer::connect(const char* peer, Microseconds timeout)
{
    // If the gameboy player is active, any multiplayer initialization would
    // clobber the Normal_32 serial transfer between the gameboy player and the
    // gameboy advance.
    // if (get_gflag(GlobalFlag::gbp_unlocked)) {
    //     return;
    // }

    multiplayer_init(timeout);
}


void Platform::NetworkPeer::listen(Microseconds timeout)
{
    // if (get_gflag(GlobalFlag::gbp_unlocked)) {
    //     return;
    // }

    multiplayer_init(timeout);
}


void Platform::NetworkPeer::update()
{
}


static int last_tx_count = 0;


Platform::NetworkPeer::Stats Platform::NetworkPeer::stats()
{
    auto& mc = multiplayer_comms;

    const int empty_transmits = mc.null_bytes_written / max_message_size;
    mc.null_bytes_written = 0;

    Float link_saturation = 0.f;

    if (empty_transmits) {
        auto tx_diff = mc.tx_message_count - last_tx_count;

        link_saturation = Float(tx_diff) / (empty_transmits + tx_diff);
    }

    last_tx_count = mc.tx_message_count;

    return {mc.tx_message_count,
            mc.rx_message_count,
            mc.tx_loss,
            mc.rx_loss,
            static_cast<int>(100 * link_saturation)};
}


bool Platform::NetworkPeer::supported_by_device()
{
    return true;
}


bool Platform::NetworkPeer::is_connected() const
{
    return multiplayer_connected;
}


bool Platform::NetworkPeer::is_host() const
{
    return multiplayer_comms.is_host;
}


void Platform::NetworkPeer::disconnect()
{
    // Be very careful editing this function. We need to get ourselves back to a
    // completely clean slate, otherwise, we won't be able to reconnect (e.g. if
    // you leave a message sitting in the transmit ring, it may be erroneously
    // sent out when you try to reconnect, instead of the handshake message);
    if (is_connected()) {

        // irqSet(IRQ_TIMER1, audio_update_fast_isr);

        info(*::platform, "disconnected!");
        multiplayer_connected = false;
        irqDisable(IRQ_SERIAL);
        REG_SIOCNT = 0;

        auto& mc = multiplayer_comms;

        if (mc.poller_current_message) {
            // Not sure whether this is the correct thing to do here...
            mc.rx_message_pool.post(mc.poller_current_message);
            mc.poller_current_message = nullptr;
        }

        mc.rx_iter_state = 0;
        if (mc.rx_current_message) {
            mc.rx_message_pool.post(mc.rx_current_message);
            mc.rx_current_message = nullptr;
        }
        mc.rx_current_all_zeroes = true;
        for (auto& msg : mc.rx_ring) {
            if (msg) {
                mc.rx_message_pool.post(msg);
                msg = nullptr;
            }
        }
        mc.rx_ring_write_pos = 0;
        mc.rx_ring_read_pos = 0;

        mc.tx_iter_state = 0;
        if (mc.tx_current_message) {
            mc.tx_message_pool.post(mc.tx_current_message);
            mc.tx_current_message = nullptr;
        }
        for (auto& msg : mc.tx_ring) {
            if (msg) {
                mc.tx_message_pool.post(msg);
                msg = nullptr;
            }
        }
        mc.tx_ring_write_pos = 0;
        mc.tx_ring_read_pos = 0;
    }
}


Platform::NetworkPeer::Interface Platform::NetworkPeer::interface() const
{
    return Interface::serial_cable;
}


Platform::NetworkPeer::~NetworkPeer()
{
    // ...
}


////////////////////////////////////////////////////////////////////////////////
// SystemClock
//
// Uses the cartridge RTC hardware, over the gpio port.
//
////////////////////////////////////////////////////////////////////////////////


static void rtc_gpio_write_command(u8 value)
{
    u8 temp;

    for (u8 i = 0; i < 8; i++) {
        temp = ((value >> (7 - i)) & 1);
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 5;
    }
}


[[gnu::
      unused]] // Currently unused, but this is how you would write to the chip...
static void
rtc_gpio_write_data(u8 value)
{
    u8 temp;

    for (u8 i = 0; i < 8; i++) {
        temp = ((value >> i) & 1);
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 4;
        S3511A_GPIO_PORT_DATA = (temp << 1) | 5;
    }
}


static u8 rtc_gpio_read_value()
{
    u8 temp;
    u8 value = 0;

    for (u8 i = 0; i < 8; i++) {
        S3511A_GPIO_PORT_DATA = 4;
        S3511A_GPIO_PORT_DATA = 4;
        S3511A_GPIO_PORT_DATA = 4;
        S3511A_GPIO_PORT_DATA = 4;
        S3511A_GPIO_PORT_DATA = 4;
        S3511A_GPIO_PORT_DATA = 5;

        temp = ((S3511A_GPIO_PORT_DATA & 2) >> 1);
        value = (value >> 1) | (temp << 7);
    }

    return value;
}


static u8 rtc_get_status()
{
    S3511A_GPIO_PORT_DATA = 1;
    S3511A_GPIO_PORT_DATA = 5;
    S3511A_GPIO_PORT_DIRECTION = 7;

    rtc_gpio_write_command(S3511A_CMD_STATUS | S3511A_RD);

    S3511A_GPIO_PORT_DIRECTION = 5;

    const auto status = rtc_gpio_read_value();

    S3511A_GPIO_PORT_DATA = 1;
    S3511A_GPIO_PORT_DATA = 1;

    return status;
}


static auto rtc_get_datetime()
{
    std::array<u8, 7> result;

    S3511A_GPIO_PORT_DATA = 1;
    S3511A_GPIO_PORT_DATA = 5;
    S3511A_GPIO_PORT_DIRECTION = 7;

    rtc_gpio_write_command(S3511A_CMD_DATETIME | S3511A_RD);

    S3511A_GPIO_PORT_DIRECTION = 5;

    for (auto& val : result) {
        val = rtc_gpio_read_value();
    }

    result[4] &= 0x7F;

    S3511A_GPIO_PORT_DATA = 1;
    S3511A_GPIO_PORT_DATA = 1;

    return result;
}


Platform::SystemClock::SystemClock()
{
}


static u32 bcd_to_binary(u8 bcd)
{
    if (bcd > 0x9f)
        return 0xff;

    if ((bcd & 0xf) <= 9)
        return (10 * ((bcd >> 4) & 0xf)) + (bcd & 0xf);
    else
        return 0xff;
}


std::optional<DateTime> Platform::SystemClock::now()
{
    if (rtc_faulty) {
        return {};
    }

    REG_IME = 0; // hopefully we don't miss anything important, like a serial
                 // interrupt! But nothing should call SystemClock::now() very
                 // often...

    const auto [year, month, day, dow, hr, min, sec] = rtc_get_datetime();

    REG_IME = 1;

    DateTime info;
    info.date_.year_ = bcd_to_binary(year);
    info.date_.month_ = bcd_to_binary(month);
    info.date_.day_ = bcd_to_binary(day);
    info.hour_ = bcd_to_binary(hr);
    info.minute_ = bcd_to_binary(min);
    info.second_ = bcd_to_binary(sec);

    return info;
}


void Platform::SystemClock::init(Platform& pfrm)
{
    S3511A_GPIO_PORT_READ_ENABLE = 1;

    auto status = rtc_get_status();
    if (status & S3511A_STATUS_POWER) {
        warning(pfrm, "RTC chip power failure");
    }
}


////////////////////////////////////////////////////////////////////////////////
// RemoteConsole
////////////////////////////////////////////////////////////////////////////////


bool Platform::RemoteConsole::supported_by_device()
{
    return false;
}

bool Platform::RemoteConsole::readline(bool (*callback)(Platform&, const char*))
{
    return false;
}

void Platform::RemoteConsole::print(const char* text)
{
}


#endif // __GBA__
