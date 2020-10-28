# BPCore-Engine

This repository includes parts of the BlindJump C++ engine, hacked together with a Lua interpreter, with the intention of allowing people to make gameboy games without needing to write C++ code or to use a compiler. The lua API for BPCore uses the simple APIs of fantasy consoles, like Pico8 or Tic80, as a model. In fact, many of the commands, like `spr()` and `btn()`, are almost the same.

*NOTE*: This project is a work-in-progress. See [Status](#status).

# Architecture

For ease of use, the `build.lua` script allows you to create Gameboy Advance ROMs entirely with Lua: you only need a copy of the `build.lua` script, a copy of the `BPCoreEngine.gba` rom, and an installation of Lua 5.3!

But how does this actually work?

The `build.lua` script parses a user-defined `manifest.lua` file, which tells the build system which resources to include in the ROM. A manifest will look something like this:
```lua

local app = {
   name = "TestApplication",
   files = {
      -- some music
      "music_test.raw",

      -- entry point script
      "main.lua",

      -- some image files
      "overlay.bmp",
      "tile0.bmp",
   }
}

return app
```

`build.lua` then creates a ROM file, by copying the compiled code in the BPCoreEngine.gba ROM, and appending a new section to the ROM, containing all of the resource files. The engine, upon startup, scans through the appropriate memory sections in the GBA cartridge address space, and finds the resource bundle. BPCore then loads the `main.lua` script from the application bundle, and turns over control to Lua (more or less, the engine does still process interrupts).

# API

* `clear()`
Clear all sprites from the screen. Should be called once per frame. `clear()` also performs a VSync, so all game updates should be performed before the clear call, and all draw calls should be placed after the clear call. For performance reasons, `clear()` does not erase tiles from the screen. `tile()` calls, and by extension, `text()` calls, are persistent.

* `display()`
Show any recent `spr()` and `tile()` calls.

* `delta()`
Returns time since the last delta call in microseconds. Because games written in Lua may push the Gameboy CPU to its limits, games may not run at a steady framerate. You can either carefully fine-tune your game to run at a specific framerate, or, scale game updates based on the frame delta.

* `btn(num)`
Returns true if the button associated with `num` is pressed. Button ids: A-0, B-1, start-2, select-3, left-4, right-5, up-6, down-7, lbumper-8, rbumper-9.

* `btnp(num)`
Returns true if the button associated with `num` transitioned from unpressed to pressed.

* `btnnp(num)`
Returns true if the button associated with `num` transitioned from pressed to unpressed.

* `text(string, x, y, [foreground color hex], [background color hex])`
Render text to the overlay tile layer, using the system font. Supports Utf-8, although the engine does not include the whole universe of unicode glyphs, for practical reasons. BPCore ships with english alphanumeric characters, accented characters for Spanish and French, and Japanese Katakana. By default, the string will use color indices 2 and 3 in the overlay layer's palette, but you can also use custom color ids. Note that x and y refer to tile layer coordinates in the overlay, not absolute screen pixels offsets.

* `txtr(layer, filename)`
Load image data from the resource bundle into VRAM. Layer refers to either the spritesheet, or one of the game's tile layers; layer0: overlay, layer1: map1, layer2: map0, layer4: spritesheet. The resource limits for the different layers vary, and will be enumerated here later. The overlay is drawn first, followed by sprites, followed by the two map layers. There is in fact another layer available, background layer3, which shares texture memory with layer2. layer3 and the overlay layer are 32x32 tiles in size, where each tile is 8x8 pixels in size. Map1 and Map0 are 64x64 tiles in size.

* `spr(index, x, y, [xflip], [yflip])`
Draw `index` from the spritesheet at screen pixel offset (`x`,`y`). Includes optional flipping flags. The spr command is unfinished, but all sprites will be 16x16 pixels in size.

* `tile(layer, tile_num, x, y)`
Draw tile indicated by `tile_num` in tile layer `layer`, with coordinates `x` and `y`. Unlike `spr()`, tiles are persistent, and do not need to be redrawn for each frame.

* `poke(address, byte)`
Set byte at address.

* `poke4(address, word)`
Set word at address.

* `peek(address)`
Returns byte value at address.

* `peek4(address)`
Returns word value at address.

* `fade(amount, [custom_color_hex], [include_sprites], [include_overlay])`
Fade the screen. Amount should be in the range `0.0` to `1.0`.

# Example

``` Lua

-- play some music from a 16 kHz signed 8-bit pcm wave file
music("music_as_colorful_as_ever.raw", 0)

-- Fade the screen while we load a texture and fill the tile layer.
fade(1)

-- Load tileset from tile0.bmp into VRAM for the tile0 layer (layer 2).
txtr(2, "tile0.bmp")

-- Fill the tile0 map with some tiles.
for i = 0, 63 do
   for j = 0, 63 do
      tile(2, 1, i, j)
   end
end

fade(0)

-- Load texture for sprites into VRAM.
txtr(4, "spritesheet")

function main_loop(update, draw)
   while true do
      update(delta())
      clear()
      draw()
      display()
   end
end

local x = 0
local dir = 0

function update(dt)
   -- fade the screen based on button presses
   if btnp(6) then
      fade(0.5)
   elseif btnnp(6) then
      fade(0)
   end

   -- move the character back and forth
   if dir == 0 then
      if x < 240 then
         x = x + 1
      else
         dir = 1
      end
   else
      if x > 0 then
         x = x - 1
      else
         dir = 0
      end
   end
end


function draw()
   -- draw a sprite for our character
   spr(15, x, 60)
end

-- Let's show how much ram we're using
text(tostring(collectgarbage("count") * 1024), 3, 5)


-- enter main loop
main_loop(update, draw)


```


# Status

This project is not very far along yet, mostly just a minimal outline for proof of concept. The ROM builds, of course, and supports all of the same features as the Blind Jump GBA engine. After building the ROM, you can use a script, `build.lua` to copy the contents of your lua scripts into the compiled ROM, and then the engine will search the ROM for the appended resource bundle, and execute the Lua code. So, the project works as a proof of concept, but until we have support for texture mapping from files in the resource bundle, you cannot really create a game with this engine yet. But at least we have a ROM skeleton, into which we can copy lua scripts, and we've got the Lua interpreter running on a gameboy, so that's a start!

But, anyway, all of the source code, from Blind Jump, exists for the following features, it just needs to be hooked into Lua:
* Link cable multiplayer
* Digital sound mixer
* Views and scrolling
* Unicode text engine
* Fades and other color palette effects
