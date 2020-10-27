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


static void show_debug_image()
{
    platform->load_overlay_texture("overlay");
    platform->load_tile1_texture("tilesheet_top");
    platform->load_tile0_texture("title_1_flattened");

    draw_image(*platform, 1, 0, 3, 30, 14, Layer::background);

    const auto screen_tiles = calc_screen_tiles(*platform);

    for (int i = 0; i < screen_tiles.x; ++i) {
        platform->set_tile(Layer::background, i, 2, 9);
    }

    for (int i = 11; i < 23; ++i) {
        platform->set_tile(Layer::background, i, 3, 9);
    }

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 20; ++y) {
            platform->set_tile(Layer::map_0, x, y, 1);
            platform->set_tile(Layer::map_1, x, y, 0);
        }
    }
}


static const struct {
    const char* name_;
    int (*callback_)(lua_State*);
} builtins[] = {
    {"logint",
     [](lua_State* L) -> int {
         char buffer[32];
         english__to_string(lua_tonumber(L, 1), buffer, 10);
         info(*platform, buffer);
         return 0;
     }},
    {"connect",
     [](lua_State* L) -> int {
         if (platform->network_peer().is_connected()) {
             platform->network_peer().disconnect();
         }
         platform->network_peer().connect(nullptr);
         return 0;
     }},
    {"disconnect",
     [](lua_State* L) -> int {
         platform->network_peer().disconnect();
         return 0;
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
     {"getdelta",
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
     {"text",
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
     {"tilesheet",
      [](lua_State* L) -> int {
          [[gnu::unused]]
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
      }}
};


static void show_lua_error(lua_State* lua,
                           const char* heading,
                           const char* error)
{
    platform->screen().clear();

    platform->enable_glyph_mode(true);
    platform->fill_overlay(112);

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

    show_lua_error(L, "Lua Panicked!", msg);

    return 0;
}


BPCoreEngine::BPCoreEngine(Platform& pf)
{
    platform = &pf;

    {
        platform->screen().clear();
        platform->enable_glyph_mode(true);
        platform->load_overlay_texture("overlay");
        pf.fill_overlay(112);
        Text loading_text(pf, "loading...", {1, 1});
        platform->screen().display();
        fs_.init(pf);
        // platform->enable_glyph_mode(false);
    }
    pf.fill_overlay(0);
    platform->screen().display();

    lua_ = lua_newstate(umm_lua_alloc, nullptr);
    lua_atpanic(lua_, &lua_panic);

    luaL_openlibs(lua_);

    show_debug_image();

    for (const auto& builtin : builtins) {
        lua_pushcfunction(lua_, builtin.callback_);
        lua_setglobal(lua_, builtin.name_);
    }

    if (auto script = fs_.get_file("main.lua")) {
        if (luaL_loadstring(lua_, script)) {
            show_lua_error(lua_, "Fatal Error: ", lua_tostring(lua_, -1));
        }
    }

    if (lua_pcall(lua_, 0, 0, 0)) {
        show_lua_error(lua_, "Fatal Error: ", lua_tostring(lua_, -1));
    }
}


void BPCoreEngine::run()
{
    auto& pf = *platform;

    pf.fatal();
}
