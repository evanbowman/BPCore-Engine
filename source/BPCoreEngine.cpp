#include "BPCoreEngine.hpp"
#include "umm_malloc/src/umm_malloc.h"
#include "graphics/overlay.hpp"
#include "localization.hpp"
#include "string.hpp"

extern "C" {
#include "lua/lualib.h"
#include "lua/lauxlib.h"
}


static Platform* platform;


static void *umm_lua_alloc(void*, void* ptr, size_t, size_t nsize)
{
    if (nsize == 0) {
        umm_free(ptr);
        return nullptr;
    } else {
        char buffer[32];
        english__to_string(nsize, buffer, 10);

        StringBuffer<64> text;
        text = "realloc ";
        text += buffer;
        text += " bytes";

        info(*platform, text.c_str());

        return umm_realloc(ptr, nsize);
    }
}


static const struct {
    const char* name_;
    int (*callback_)(lua_State*);
} builtins[] = {
    {"log",
     [](lua_State* L) -> int {
         info(*platform, lua_tostring(L, 1));
         return 0;
     }},
    {"connect",
     [](lua_State* L) -> int {
         if (platform->network_peer().is_connected()) {
             platform->network_peer().disconnect();
         }
         platform->network_peer().connect(nullptr);
         lua_pushboolean(L, platform->network_peer().is_connected());
         return 1;
     }},
    {"disconnect",
     [](lua_State* L) -> int {
         platform->network_peer().disconnect();
         return 0;
     }},
    {"send",
     [](lua_State* L) -> int {
         char message[Platform::NetworkPeer::max_message_size];
         __builtin_memset(message + 1, 0, (sizeof message) - 1);
         message[0] = 1;

         const char* str = lua_tostring(L, 1);
         const auto len = str_len(str);

         if (len > (sizeof message) - 1) {
             lua_pushboolean(L, false);
             return 1;
         }

         __builtin_memcpy(message + 1, str, len);

         Platform::NetworkPeer::Message packet;
         packet.data_ = (const byte*)message;
         packet.length_ = sizeof message;

         while (not platform->network_peer().send_message(packet)) {
             if (not platform->network_peer().is_connected()) {
                 lua_pushboolean(L, false);
                 return 1;
             }
         }

         lua_pushboolean(L, true);
         return 1;
     }},
    {"recv",
     [](lua_State* L) -> int {
         if (auto message = platform->network_peer().poll_message()) {
             lua_pushlstring(L, (const char*)message->data_ + 1,
                             Platform::NetworkPeer::max_message_size - 1);
             platform->
                 network_peer()
                 .poll_consume(Platform::NetworkPeer::max_message_size);
             return 1;
         }
         lua_pushnil(L);
         return 1;
     }},
    {"clear",
     [](lua_State* L) -> int {
         platform->feed_watchdog();
         platform->network_peer().update();
         platform->screen().clear();
         return 0;
     }},
     {"display",
      [](lua_State* L) -> int {
          platform->screen().display();
          platform->keyboard().poll();
          return 0;
      }},
     {"delta",
      [](lua_State* L) -> int {
          lua_pushnumber(L, platform->delta_clock().reset());
          return 1;
      }},
     {"btn",
      [](lua_State* L) -> int {
          const int button = lua_tonumber(L, 1);
          if (button >= static_cast<int>(Key::count)) {
              lua_pushboolean(L, false);
          } else {
              const auto k = static_cast<Key>(button);
              lua_pushboolean(L, platform->keyboard().pressed(k));
          }
          return 1;
      }},
     {"btnp",
      [](lua_State* L) -> int {
          const int button = lua_tonumber(L, 1);
          if (button >= static_cast<int>(Key::count)) {
              lua_pushboolean(L, false);
          } else {
              const auto k = static_cast<Key>(button);
              lua_pushboolean(L, platform->keyboard().down_transition(k));
          }
          return 1;
      }},
     {"btnnp",
      [](lua_State* L) -> int {
          const int button = lua_tonumber(L, 1);
          if (button >= static_cast<int>(Key::count)) {
              lua_pushboolean(L, false);
          } else {
              const auto k = static_cast<Key>(button);
              lua_pushboolean(L, platform->keyboard().up_transition(k));
          }
          return 1;
      }},
     {"print",
      [](lua_State* L) -> int {
          const int argc = lua_gettop(L);
          Text::OptColors c;
          if (argc > 3) {
              c.emplace();
              c->foreground_ =
                  static_cast<ColorConstant>(lua_tointeger(L, 4));
          }
          if (argc > 4) {
              c->background_ =
                  static_cast<ColorConstant>(lua_tointeger(L, 5));
          }
          print_str(*platform, lua_tostring(L, 1),
                    OverlayCoord {
                        (u8)lua_tonumber(L, 2),
                        (u8)lua_tonumber(L, 3)
                    }, c);
          return 0;
      }},
     {"txtr",
      [](lua_State* L) -> int {
          const int layer = lua_tonumber(L, 1);
          const char* filename = lua_tostring(L, 2);

          switch (static_cast<Layer>(layer)) {
          case Layer::overlay:
              platform->load_overlay_texture(filename);
              break;

          case Layer::map_1:
              platform->load_tile1_texture(filename);
              break;

          case Layer::map_0:
              platform->load_tile0_texture(filename);
              break;

          default:
              if (layer == 4) {
                  platform->load_sprite_texture(filename);
              }
              break;
          }

          return 0;
      }},
     {"spr",
      [](lua_State* L) -> int {
          Sprite spr;
          spr.set_texture_index(lua_tointeger(L, 1));
          spr.set_position({
                  Float(lua_tonumber(L, 2)),
                  Float(lua_tonumber(L, 3))
              });

          const int argc = lua_gettop(L);
          if (argc > 3) {
              const bool xflip = lua_toboolean(L, 4);
              if (argc > 4) {
                  bool yflip = lua_toboolean(L, 5);
                  spr.set_flip({xflip, yflip});
              }
              spr.set_flip({xflip, false});
          }

          platform->screen().draw(spr);

          return 0;
      }},
     {"priority",
      [](lua_State* L) {
          const int s = lua_tointeger(L, 1);
          const int b = lua_tointeger(L, 2);
          const int t0 = lua_tointeger(L, 3);
          const int t1 = lua_tointeger(L, 4);

          platform->set_priorities(s, b, t0, t1);

          return 0;
      }},
     {"scroll",
      [](lua_State* L) -> int {
          const int l = lua_tointeger(L, 1);
          const int x = lua_tointeger(L, 2);
          const int y = lua_tointeger(L, 3);

          platform->scroll(static_cast<Layer>(l), x, y);

          return 0;
      }},
     {"camera",
      [](lua_State* L) -> int {
          const int x = lua_tonumber(L, 1);
          const int y = lua_tonumber(L, 2);

          auto view = platform->screen().get_view();
          const auto& screen_size = platform->screen().size();

          view.set_center({Float(x - int(screen_size.x / 2)),
                           Float(y - int(screen_size.y / 2))});

          platform->screen().set_view(view);

          return 0;
      }},
     {"tile",
      [](lua_State* L) -> int {
          const int argc = lua_gettop(L);
          const int l = lua_tointeger(L, 1);
          const int x = lua_tointeger(L, 2);
          const int y = lua_tointeger(L, 3);

          if (argc == 4) {
              const int t = lua_tointeger(L, 4);

              switch (static_cast<Layer>(l)) {
              case Layer::overlay:
                  // NOTE: The first 82 tiles in the overlay graphics layer are
                  // reserved for glyphs. We adjust the user-supplied indices
                  // accordingly.
                  platform->set_tile(static_cast<Layer>(l), x, y, t + 83);
                  break;

              case Layer::map_1:
              case Layer::map_0:
              case Layer::background:
                  platform->set_tile(static_cast<Layer>(l), x, y, t);
                  break;
              }
              return 0;
          } else {
              switch (static_cast<Layer>(l)) {
              case Layer::overlay: {
                  auto t = platform->get_tile(static_cast<Layer>(l), x, y);
                  if (t <= 82) {
                      // The engine does not allow users to capture the tile
                      // index of a glyph.
                      t = 0;
                  } else {
                      t -= 83;
                  }
                  lua_pushinteger(L, t);
                  return 1;
              }

              case Layer::map_1:
              case Layer::map_0:
              case Layer::background:
                  lua_pushinteger(L, platform->get_tile(static_cast<Layer>(l), x, y));
                  return 1;
              }
          }
          return 0;
      }},
     {"fill",
      [](lua_State* L) -> int {
          const int l = lua_tointeger(L, 1);
          const int t = lua_tointeger(L, 2);
          switch (static_cast<Layer>(l)) {
          case Layer::overlay:
              platform->fill_overlay(t);
              break;

          // TODO: implement fill for other layers...
          default:
              break;
          }
          return 0;
      }},
     {"poke",
      [](lua_State* L) -> int {
          const auto addr = lua_tointeger(L, 1);
          const u8 val = lua_tointeger(L, 2);
          *((u8*)addr) = val;
          return 0;
      }},
     {"poke4",
      [](lua_State* L) -> int {
          const auto addr = lua_tointeger(L, 1);
          const u32 val = lua_tointeger(L, 2);
          *((u32*)addr) = val;
          return 0;
      }},
     {"peek",
      [](lua_State* L) -> int {
          const auto addr = lua_tointeger(L, 1);
          lua_pushinteger(L, *((u8*)addr));
          return 1;
      }},
     {"peek4",
      [](lua_State* L) -> int {
          const auto addr = lua_tointeger(L, 1);
          lua_pushinteger(L, *((u32*)addr));
          return 1;
      }},
     {"music",
      [](lua_State* L) -> int {
          const auto name = lua_tostring(L, 1);
          const auto offset = lua_tointeger(L, 2);
          platform->speaker().play_music(name, offset);
          return 0;
      }},
     {"stop_music",
      [](lua_State* L) -> int {
          platform->speaker().stop_music();
          return 0;
      }},
     {"sound",
      [](lua_State* L) -> int {
          const auto name = lua_tostring(L, 1);
          const auto priority = lua_tointeger(L, 2);
          platform->speaker().play_sound(name, priority);
          return 0;
      }},
     {"file",
      [](lua_State* L) -> int {
          const auto name = lua_tostring(L, 1);
          auto f = platform->fs().get_file(name);
          if (f.data_) {
              lua_pushinteger(L, (size_t)f.data_);
              lua_pushinteger(L, (u32)f.size_);
              return 2;
          } else {
              lua_pushnil(L);
              lua_pushinteger(L, 0);
              return 2;
          }
      }},
     {"fade",
      [](lua_State* L) -> int {
          const auto amount = lua_tonumber(L, 1);
          const int argc = lua_gettop(L);

          switch (argc) {
          case 1:
              platform->screen().fade(amount);
              break;

          case 2:
              platform->screen().fade(amount,
                                      custom_color(lua_tonumber(L, 2)));
              break;

          case 3:
              platform->screen().fade(amount,
                                      custom_color(lua_tonumber(L, 2)),
                                      {},
                                      lua_toboolean(L, 3));
              break;

          case 4:
              platform->screen().fade(amount,
                                      custom_color(lua_tonumber(L, 2)),
                                      {},
                                      lua_toboolean(L, 3),
                                      lua_toboolean(L, 4));
              break;
          }
          return 0;
      }}
};


static void fatal_error(const char* heading,
                        const char* error)
{
    platform->load_overlay_texture("overlay_text_key");

    platform->screen().clear();

    platform->enable_glyph_mode(true);
    platform->screen().fade(1.f);

    Text t(*platform, heading, {1, 1});

    TextView tv(*platform);
    tv.assign(error, {1, 4}, {28, 18});

    platform->screen().display();

    while (true) {
        platform->feed_watchdog();
    }
}


static int lua_panic(lua_State* L)
{
    const char* msg = lua_tostring(L, -1);
    if (msg == nullptr) {
        msg = "error object is not a string";
    }

    fatal_error("Lua Panicked!", msg);

    return 0;
}


BPCoreEngine::BPCoreEngine(Platform& pf)
{
    platform = &pf;
    {
        platform->screen().clear();
        platform->enable_glyph_mode(true);
        platform->screen().fade(1.f);
        Text loading_text(pf, "loading...", {1, 18});
        platform->screen().display();
        if (not pf.fs().init(pf)) {
            fatal_error("Fatal Error:",
                        "BPCore Engine failed to load resource bundle!");
        }
        // platform->enable_glyph_mode(false);
    }
    platform->screen().fade(0.f);
    platform->screen().display();

    lua_ = lua_newstate(umm_lua_alloc, nullptr);
    lua_atpanic(lua_, &lua_panic);

    luaL_openlibs(lua_);

    for (const auto& builtin : builtins) {
        lua_pushcfunction(lua_, builtin.callback_);
        lua_setglobal(lua_, builtin.name_);
    }

    if (auto script = pf.fs().get_file("main.lua").data_) {
        if (luaL_loadstring(lua_, script)) {
            fatal_error("Fatal Error: ", lua_tostring(lua_, -1));
        }
    }

    if (lua_pcall(lua_, 0, 0, 0)) {
        fatal_error("Fatal Error: ", lua_tostring(lua_, -1));
    }
}


void BPCoreEngine::run()
{
    auto& pf = *platform;

    pf.fatal();
}
