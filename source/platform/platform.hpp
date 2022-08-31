#pragma once


#include "dateTime.hpp"
#include "filesystem.hpp"
#include "function.hpp"
#include "graphics/contrast.hpp"
#include "graphics/sprite.hpp"
#include "graphics/view.hpp"
#include "key.hpp"
#include "memory/buffer.hpp"
#include "memory/rc.hpp"
#include "number/numeric.hpp"
#include "severity.hpp"
#include "sound.hpp"
#include "unicode.hpp"
#include <array>
#include <optional>


using TileDesc = u16;


struct FontColors {
    ColorConstant foreground_;
    ColorConstant background_;
};


// Anything platform specific should be defined here.


////////////////////////////////////////////////////////////////////////////////
// Platform
////////////////////////////////////////////////////////////////////////////////


enum class Layer { overlay, map_1, map_0, background };


class Platform {
public:
    class Screen;
    class Keyboard;
    class Logger;
    class Speaker;
    class NetworkPeer;
    class DeltaClock;
    class SystemClock;
    class RemoteConsole;

    using DeviceName = StringBuffer<23>;
    DeviceName device_name() const;

    // Timestamp recorded when the process launched, returns an empty optional
    // if the platform does not provide a functional clock.
    std::optional<DateTime> startup_time() const;

    inline Filesystem& fs()
    {
        return fs_;
    }

    inline Screen& screen()
    {
        return screen_;
    }

    inline Keyboard& keyboard()
    {
        return keyboard_;
    }

    inline Logger& logger()
    {
        return logger_;
    }

    inline Speaker& speaker()
    {
        return speaker_;
    }

    inline NetworkPeer& network_peer()
    {
        return network_peer_;
    }

    inline RemoteConsole& remote_console()
    {
        return console_;
    }

    inline DeltaClock& delta_clock()
    {
        return delta_clock_;
    }

    inline SystemClock& system_clock()
    {
        return system_clock_;
    }

    void set_priorities(int sprite_prior,
                        int background_prior,
                        int tile0_prior,
                        int tile1_prior);

    // NOTE: I must admit, the platform class interface has become quite
    // bloated.

    const char* get_opt(char opt);

    // On some platforms, fatal() will trigger a soft reset. But soft-reset is
    // mostly reserved for bare-metal platforms, where it's usually easy to
    // perform a soft reset via a simple BIOS call to reset the ROM. When
    // running as a windowed application within an OS, though, it's not as
    // simple to reset a process back to its original state at time of creation,
    // so it's easier in those cases to just exit. In any event, fatal() does
    // not return.
    [[noreturn]] void fatal();

    // Enable platform specific features. NOP if unsupported.
    void enable_feature(const char* feature_name, bool enabled);

    struct TextureMapping {
        const char* texture_name_;
        u16 offset_;
    };

    // Supplied with a unicode codepoint, this function should provide an offset
    // into a texture image from which to load a glyph image.
    using TextureCpMapper =
        std::optional<TextureMapping> (*)(const utf8::Codepoint&);

    // Map a glyph into the vram space reserved for the overlay tile layer.
    TileDesc map_glyph(const utf8::Codepoint& glyph, TextureCpMapper);

    // In glyph mode, the platform will automatically unmap glyphs when their
    // tiles are overwritten by set_tile.
    void enable_glyph_mode(bool enabled);


    // NOTE: For the overlay and background, the tile layers consist of 32x32
    // tiles, where each tiles is 8x8 pixels. The overlay and the background
    // wrap when scrolled. Map tiles, on the other hand, are 32x24 pixels, and
    // the whole map consists of 64x64 8x8 pixel tiles.
    void set_tile(Layer layer, u16 x, u16 y, TileDesc val);

    // A special version of set_tile meant for glyphs. Allows you to set custom
    // colors. If the platform runs out of room for colors, the oldest ones will
    // be overwritten.
    //
    // NOTE: Custom colored fonts are currently incompatible with screen
    // fades. Custom colored text will not be faded.
    void set_tile(u16 x, u16 y, TileDesc glyph, const FontColors& colors);

    // This function is not necessarily implemented efficiently, may in fact be
    // very slow.
    TileDesc get_tile(Layer layer, u16 x, u16 y);


    void scroll(Layer layer, u16 xscroll, u16 yscroll);


    void fill_overlay(u16 TileDesc);

    void set_overlay_origin(Float x, Float y);

    using FailureReason = StringBuffer<48>;

    std::optional<FailureReason> load_sprite_texture(const char* name,
                                                     int addr,
                                                     int len);

    std::optional<FailureReason> load_tile0_texture(const char* name,
                                                    int addr,
                                                    int len);

    std::optional<FailureReason> load_tile1_texture(const char* name,
                                                    int addr,
                                                    int len);

    std::optional<FailureReason> load_overlay_texture(const char* name,
                                                      int addr,
                                                      int len);

    // Sleep halts the game for an amount of time equal to some number
    // of game updates. Given that the game should be running at
    // 60fps, one update equals 1/60 of a second.
    using Frame = u32;
    void sleep(Frame frames);


    bool is_running() const;


    // If the watchdog is not fed every ten seconds, the game will reset itself,
    // after calling the user-supplied watchdog handler (obviously, don't spend
    // more than ten seconds in the watchdog handler!).
    void feed_watchdog();

    using WatchdogCallback = Function<16, void(Platform& pfrm)>;
    void on_watchdog_timeout(WatchdogCallback callback);


    // Not implemented for all platforms. If unimplemented, the funciton will
    // simply return immediately. For handheld consoles without an operating
    // system, where the only way that you can shutdown the system is by
    // flipping the power switch, it doesn't make sense to implement an exit
    // function.
    void soft_exit();

    bool write_save_data(const void* data, u32 length);
    bool read_save_data(void* buffer, u32 data_length);

#ifdef __GBA__
#define SCRATCH_BUFFER_SIZE 1200
#else
#define SCRATCH_BUFFER_SIZE 4000
#endif // __GBA__

    struct ScratchBuffer {
        // NOTE: do not make any assumptions about the alignment of the data_
        // member.
        char data_[SCRATCH_BUFFER_SIZE];
    };

    static constexpr const int scratch_buffer_count = 4;
    using ScratchBufferPtr = Rc<ScratchBuffer, scratch_buffer_count>;

    // Scratch buffers are sort of a blunt instrument. Designed for uncommon
    // scenarios where you need a lot of memory. The platform provides one
    // hundred scratch buffers to work with. The Rc wrapper will automatically
    // deallocate buffers when you're done with them. Creating a new scratch
    // buffer when the buffer pool is exhausted will cause the system to lock
    // up, so do not try to hold more than one hundred active references to
    // scratch buffers (not sure why you would even need 100kB of temporary
    // scratch space anyway...).
    ScratchBufferPtr make_scratch_buffer();

    int scratch_buffers_remaining();


    ////////////////////////////////////////////////////////////////////////////
    // DeltaClock
    ////////////////////////////////////////////////////////////////////////////


    class DeltaClock {
    public:
        ~DeltaClock();

        Microseconds reset();


        using TimePoint = int;

        TimePoint sample() const;


        static Microseconds duration(TimePoint t1, TimePoint t2);


    private:
        friend class Platform;

        DeltaClock();

        void* impl_;
    };


    ////////////////////////////////////////////////////////////////////////////
    // SystemClock
    ////////////////////////////////////////////////////////////////////////////


    class SystemClock {
    public:
        // NOTE: SystemClock::now() sometimes causes minor audio bugs on the
        // GAMEBOY ADVANCE. In the meantime, use Platform::startup_time() to
        // access an initial startup timestamp, and only call DateTime::now()
        // during shutdown, when saving a game.
        std::optional<DateTime> now();

    private:
        friend class Platform;

        void init(Platform& pfrm);

        SystemClock();
    };


    ////////////////////////////////////////////////////////////////////////////
    // Screen
    ////////////////////////////////////////////////////////////////////////////


    class Screen {
    public:
        static constexpr u32 sprite_limit = 128;

        void draw(const Sprite& spr);

        void clear();

        void display();

        Vec2<u32> size() const;

        void set_contrast(Contrast contrast);

        Contrast get_contrast() const;

        void enable_night_mode(bool enabled);

        void set_view(const View& view)
        {
            view_ = view;
        }

        const View& get_view() const
        {
            return view_;
        }

        // Blend color into sprite existing screen colors, unless a base color
        // is specified, in which case, computes the resulting color from the
        // base color blended with the color parameter.
        void fade(float amount,
                  ColorConstant color = ColorConstant::rich_black,
                  std::optional<ColorConstant> base = {},
                  bool include_sprites = true,
                  bool include_overlay = false);

        void pixelate(u8 amount,
                      bool include_overlay = true,
                      bool include_background = true,
                      bool include_sprites = true);

    private:
        Screen();

        void init_layers(int background_priority,
                         int tile0_priority,
                         int tile1_priority);

        friend class Platform;

        View view_;
        void* userdata_;
    };


    ////////////////////////////////////////////////////////////////////////////
    // Keyboard
    ////////////////////////////////////////////////////////////////////////////


    class Keyboard {
    private:
        using KeyStates = std::array<bool, int(Key::count)>;

    public:
        void poll();

        using RestoreState = Bitvector<KeyStates{}.size()>;

        template <Key... k> bool all_pressed() const
        {
            return (... and states_[int(k)]);
        }

        template <Key... k> bool any_pressed() const
        {
            return (... or states_[int(k)]);
        }

        template <Key k> bool pressed() const
        {
            return states_[int(k)];
        }

        bool pressed(Key k) const
        {
            return states_[int(k)];
        }

        bool down_transition(Key k) const
        {
            return states_[int(k)] and not prev_[int(k)];
        }

        template <Key... k> bool down_transition() const
        {
            return (... or down_transition_helper<k>());
        }

        bool up_transition(Key k) const
        {
            return not states_[int(k)] and prev_[int(k)];
        }

        template <Key k> bool up_transition() const
        {
            return not states_[int(k)] and prev_[int(k)];
        }

        RestoreState dump_state()
        {
            return states_;
        }

        void restore_state(const RestoreState& state)
        {
            // NOTE: we're assigning both the current and previous state to the
            // restored state. Otherwise, we could re-trigger a keypress that
            // already happened
            for (u32 i = 0; i < state.size(); ++i) {
                prev_[i] = state[i];
                states_[i] = state[i];
            }
        }


        struct ControllerInfo {
            int vendor_id;
            int product_id;
            int action_1_key;
            int action_2_key;
            int start_key;
            int alt_1_key;
            int alt_2_key;
        };

        void register_controller(const ControllerInfo& info);

    private:
        template <Key k> bool down_transition_helper() const
        {
            return states_[int(k)] and not prev_[int(k)];
        }

        Keyboard()
        {
            for (int i = 0; i < int(Key::count); ++i) {
                states_[i] = false;
                prev_[i] = false;
            }
        }

        friend class Platform;

        KeyStates prev_;
        KeyStates states_;
    };


    ////////////////////////////////////////////////////////////////////////////
    // Logger
    ////////////////////////////////////////////////////////////////////////////


    class Logger {
    public:
        void log(Severity severity, const char* msg);

        void read(void* buffer, u32 start_offset, u32 num_bytes);

        void set_threshold(Severity severity);

    private:
        Logger();

        friend class Platform;
    };


    ////////////////////////////////////////////////////////////////////////////
    // Speaker
    ////////////////////////////////////////////////////////////////////////////


    class Speaker {
    public:
        using Channel = int;

        void play_note(Note n, Octave o, Channel c);

        // NOTE: All music will loop. It's just more efficient to implement the
        // music such that all tracks are either looped or non-looping, and I
        // decided to make tracks loop. If you want music to stop when finished,
        // stop it yourself.
        void play_music(const char* name, Microseconds offset);
        void stop_music();

        // A platform's speaker may only have the resources to handle a limited
        // number of overlapping sounds. For such platforms, currently running
        // sounds with a lower priority will be evicted, to make room for
        // higher-priority sounds.
        //
        // If you pass in an optional position, platforms that support spatial
        // audio will attenuate the sound based on distance to the listener (the
        // camera center);
        void play_sound(const char* name,
                        int priority,
                        std::optional<Vec2<Float>> position = {});
        bool is_sound_playing(const char* name);

        // Updates the listener position for spatialized audio, if supported.
        void set_position(const Vec2<Float>& position);

        Microseconds track_length(const char* sound_or_music_name);

    private:
        friend class Platform;

        Speaker();
    };


    ////////////////////////////////////////////////////////////////////////////
    // NetworkPeer (incomplete...)
    ////////////////////////////////////////////////////////////////////////////


    class NetworkPeer {
    public:
        NetworkPeer();
        NetworkPeer(const NetworkPeer&) = delete;
        ~NetworkPeer();

        void connect(const char* peer_address, Microseconds timeout);
        void listen(Microseconds timeout);

        void disconnect();

        bool is_connected() const;
        bool is_host() const;

        struct Message {
            const byte* data_;
            u32 length_;
        };

        enum Interface {
            serial_cable,
            internet,
        };

        Interface interface() const;

        // NOTE: You cannot transmit messages larger than 12 bytes. On the
        // gameboy advance, 12 byte messages require at least six serial io
        // interrupts, along with a bunch of timer interrupts. It's just not
        // realistic to make the messages too much larger, if you want to
        // receive the data within a reasonable amount of time on all platforms.
        static const u32 max_message_size = 12;

        // IMPORTANT!!! Messages containing all zeroes are not guaranteed to be
        // received on some platforms, so you should have at least some high
        // bits in your message.
        bool send_message(const Message& message);

        void update();

        // The result of poll-message will include the length of the available
        // data in the network-peer's buffer. If the space in the buffer is
        // insufficient to frame a message, exit polling, and do not call
        // poll_consume() until there's enough space to fill an entire message.
        std::optional<Message> poll_message();
        void poll_consume(u32 length);

        // Will return false if the platform does not support networked
        // multiplayer.
        static bool supported_by_device();

        struct Stats {
            int transmit_count_;
            int receive_count_;
            int transmit_loss_;
            int receive_loss_;
            int link_saturation_; // percentage 0 to 100
        };

        Stats stats();

    private:
        void* impl_;
    };


    ////////////////////////////////////////////////////////////////////////////
    // RemoteConsole
    //
    // Some platforms, like desktop computers, support a remote console, whereby
    // the user can enter text;
    //
    ////////////////////////////////////////////////////////////////////////////


    class RemoteConsole {
    public:
        bool supported_by_device();

        // Return true if readline succeeds and callback is true, return false
        // if readline fails or if callback returns false.
        bool readline(bool (*callback)(Platform&, const char*));

        void print(const char* text);
    };


    ////////////////////////////////////////////////////////////////////////////
    // Task
    ////////////////////////////////////////////////////////////////////////////

    class Task {
    public:
        virtual void run() = 0;

        virtual ~Task()
        {
        }

        bool complete() const
        {
            return complete_;
        }

        bool running() const
        {
            return running_;
        }

    protected:
        void completed()
        {
            complete_ = true;
        }

        friend class Platform;

    private:
        Atomic<bool> running_ = false;
        Atomic<bool> complete_ = false;
    };

    // If only we had a heap, and shared pointers, we could enforce better
    // ownership than raw pointers for Tasks... ah well.
    void push_task(Task* task);


    class Data;

    Data* data()
    {
        return data_;
    }

    const Data* data() const
    {
        return data_;
    }

    Platform(const Platform&) = delete;
    ~Platform();

private:
    Platform();

    friend int main(int argc, char** argv);

    Filesystem fs_;
    SystemClock system_clock_;
    NetworkPeer network_peer_;
    DeltaClock delta_clock_;
    RemoteConsole console_;
    Screen screen_;
    Keyboard keyboard_;
    Speaker speaker_;
    Logger logger_;
    Data* data_ = nullptr;
};


class SynchronizedBase {
protected:
    void init(Platform& pf);
    void lock();
    void unlock();

    ~SynchronizedBase();

private:
    friend class Platform;

    void* impl_;
};


template <typename T> class Synchronized : SynchronizedBase {
public:
    template <typename... Args>
    Synchronized(Platform& pf, Args&&... args)
        : data_(std::forward<Args>(args)...)
    {
        init(pf);
    }

    template <typename F> void acquire(F&& handler)
    {
        lock();
        handler(data_);
        unlock();
    }

private:
    T data_;
};


// Helper function for drawing background tiles larger than the default size (8x8 pixels)
inline void draw_image(Platform& pfrm,
                       TileDesc start_tile,
                       u16 start_x,
                       u16 start_y,
                       u16 width,
                       u16 height,
                       Layer layer)
{

    u16 tile = start_tile;

    for (u16 y = start_y; y < start_y + height; ++y) {
        for (u16 x = start_x; x < start_x + width; ++x) {
            pfrm.set_tile(layer, x, y, tile++);
        }
    }
}


#ifdef __BLINDJUMP_ENABLE_LOGS
#ifdef __GBA__
// #pragma message "Warning: logging can wear down Flash memory, be careful using this on physical hardware!"
#endif
inline void debug(Platform& pf, const char* msg)
{
    pf.logger().log(Severity::debug, msg);
}
inline void info(Platform& pf, const char* msg)
{
    pf.logger().log(Severity::info, msg);
}
inline void warning(Platform& pf, const char* msg)
{
    pf.logger().log(Severity::warning, msg);
}
inline void error(Platform& pf, const char* msg)
{
    pf.logger().log(Severity::error, msg);
}
#else
inline void debug(Platform&, const char*)
{
}
inline void info(Platform&, const char*)
{
}
inline void warning(Platform&, const char*)
{
}
inline void error(Platform&, const char*)
{
}
#endif // __BLINDJUMP_ENABLE_LOGS


u16 arctangent(u16 y, u16 x);
