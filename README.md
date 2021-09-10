# BPCore-Engine

(**B**lind jum**P** **Core** Engine)

This repository includes parts of the [BlindJump](https://github.com/evanbowman/blind-jump-portable) C++ engine, hacked together with a Lua interpreter, with the intention of allowing people to make gameboy games without needing to write C++ code or to use a compiler. The lua API for BPCore uses the simple APIs of fantasy consoles, like Pico8 or Tic80, as a model. In fact, many of the commands, like `spr()` and `btn()`, are almost the same.


# Architecture

For ease of use, the `build.lua` script allows you to create Gameboy Advance ROMs entirely with Lua: you only need a copy of the `build.lua` script, a copy of the `BPCoreEngine.gba` rom, and an installation of Lua 5.3!

But how does this actually work?

The `build.lua` script parses a user-defined `manifest.lua` file, which tells the build system which resources to include in the ROM. A manifest will look something like this:
```lua

local app = {
   name = "TestApplication",

   tilesets = {
      "overlay.bmp",
      "tile0.bmp",
   },

   spritesheets = {
      "spritesheet.bmp",
   },

   audio = {
      "my_music.raw",
   },

   scripts = {
      "main.lua",
   },

   misc = {
      "some_data.txt",
   }
}

return app

```

`build.lua` then creates a ROM file, by copying the compiled code in the BPCoreEngine.gba ROM, and appending a new section to the ROM, containing all of the resource files. The engine, upon startup, loads the address of the end of the ROM (provided by the linker), and finds the resource bundle. BPCore then loads the `main.lua` script from the application bundle, and turns over control to Lua (more or less, the engine does still process interrupts).

# API

## Sprites and Tiles

The BPCore engine uses the Gameboy Advance's tile-based display mode. All sprites are 16x16 pixels in size, and all tiles are 8x8 pixels wide. The engine provides access to four tiles layers:

* The overlay: comprised of 32x32 tiles, this layer displays in front of all other sprites and tile layers. The overlay uses layer id 0. The `print()` function draws its text using tile indices in the overlay tile layer.

* Tile layer 1 (aka tile_1): a larger layer comprised of 64x64 tiles. Uses layer id 1. Displays behind sprites, but in front of all subsequent tile layers.

* Tile layer 0 (aka tile_0): a larger layer comprised of 64x64 tiles. Uses layer id 2. Displays behind sprites and tile_1, but in front of the background layer.

* The background: Another smaller layer, comprised of 32x32 tiles. Uses layer id 3. Displays behind all other layers.

To load data from the a bundled file into VRAM, use the `txtr()` function, with one of the layer ids above. To load a spritesheet, you may also use the `txtr()` function, with layer id 4.

## Function Reference

* `sleep(frames)`
Sleep the game for N frames.

* `delta()`
Returns time since the last delta call in microseconds. Because games written in Lua may push the Gameboy CPU to its limits, games may not run at a steady framerate. You can either carefully fine-tune your game to run at a specific framerate, or, scale game updates based on the frame delta.

* `btn(num)`
Returns true if the button associated with `num` is pressed. Button ids: A-0, B-1, start-2, select-3, left-4, right-5, up-6, down-7, lbumper-8, rbumper-9.

* `btnp(num)`
Returns true if the button associated with `num` transitioned from unpressed to pressed.

* `btnnp(num)`
Returns true if the button associated with `num` transitioned from pressed to unpressed.

* `print(string, x, y, [foreground color hex], [background color hex])`
Render text to the overlay tile layer, using the system font. Supports Utf-8, although the engine does not include the whole universe of unicode glyphs, for practical reasons. BPCore ships with english alphanumeric characters, accented characters for Spanish and French, and Japanese Katakana. By default, the string will use color indices 2 and 3 in the overlay layer's palette, but you can also use custom color ids. Note that x and y refer to tile layer coordinates in the overlay, not absolute screen pixels offsets.

* `txtr(layer, filename)`
Load image data from the resource bundle into VRAM. Layer refers to either the spritesheet, or one of the game's tile layers; layer0: overlay, layer1: map1, layer2: map0, layer4: spritesheet. The resource limits for the different layers vary, and will be enumerated here later. The overlay is drawn first, followed by sprites, followed by the two map layers. There is in fact another layer available, background layer3, which shares texture memory with layer2. layer3 and the overlay layer are 32x32 tiles in size, where each tile is 8x8 pixels in size. Map1 and Map0 are 64x64 tiles in size.

* `spr(index, x, y, [xflip], [yflip])`
Draw `index` from the spritesheet at screen pixel offset (`x`,`y`). Includes optional flipping flags. The spr command is unfinished, but all sprites will be 16x16 pixels in size.

* `tile(layer, x, y, [tile_num])`
Draw tile indicated by `tile_num` in tile layer `layer`, with coordinates `x` and `y`. Unlike `spr()`, tiles are persistent, and do not need to be redrawn for each frame. If called without `tile_num`, will instead return the current tile value at `x`,`y` in `layer`.

* `fade(amount, [custom_color_hex], [include_sprites], [include_overlay])`
Fade the screen. Amount should be in the range `0.0` to `1.0`.

* `camera(x, y)`
Set the center of the view.

* `scroll(layer, x_amount, y_amount)`
In addition to re-anchoring the camera, you may also manually set the scrolling for any of the tile layers. The scroll amounts for tile_0, tile_1, and the background are all relative, so they will be applied in addition to the camera scrolling. The camera does not scroll the overlay, so the scroll amounts for this layer are absolute.

* `priority(sprite_pr, background_pr, tile0_pr, tile1_pr)`
Reorder the engine's rendering layers, by assigning new priorities to the layers. You should use values 0-3 for priorities, 0 being the nearest layer to the screen, and 3 being the furthest layer. The overlay priority defaults to 0, and may not be changed. Default values upon startup: background=3, tile_0=3, tile_1 = 2, sprite=1, overlay=0. Certain layers will display behind other layers when assigned the same priority. Order of precedence, if all layers were to be assigned the same priority value: sprite > tile_0 > background > overlay > tile_1.

* `clear()`
Clear all sprites from the screen. Should be called once per frame. `clear()` also performs a VSync, so all game updates should be performed before the clear call, and all draw calls should be placed after the clear call. For performance reasons, `clear()` does not erase tiles from the screen. `tile()` calls, and by extension, `print()` calls, are persistent.

* `display()`
Show any recent `spr()` and `tile()` calls.

* `poke(address, byte)`
Set byte at address.

* `poke4(address, word)`
Set word at address.

* `peek(address)`
Returns byte value at address.

* `peek4(address)`
Returns word value at address.

* `file(name)`
Returns a pointer,length to any file in the resource bundle. The data can then be read with the peek/peek4 functions. You cannot write to files, as they reside in ROM, and are therefore, by definition, read-only.

* `music(source_file, offset)`
Play mono 16kHz signed 8bit PCM audio from the given source file string. All music loops, and you may specify a microsecond offset into the music file with the `offset` parameter.

* `sound(source_file, priority)`
Play mono 16kHz signed 8bit PCM audio from the given source file string. Unlike the music, sounds do not loop. The engine can only render four audio channels at a time--3 for sound effects, and one for the music. If you already have three sounds playing, the sound effect with the lowest priority will be evicted if the sound that you are requesting has a higher priority.

* `next_script(name)`
Execute script `name` when the current script runs to completion. Due to memory constraints (the GBA has limited RAM), you may need to structure your program as a series of isolated scripts. Each script is completetly independent, i.e. scripts start with a clean slate when they begin running. Therefore, Lua global variables may not be shared between lua scripts, so you will need to write any persistent data into an unused section of GBA RAM. While swapping scripts will erase any existing Lua code or Lua variables from RAM, starting a new script does not otherwise impact the state of the BPCore engine, so, for example, any tiles that you created in the current script, will be unchanged when moving to the next script. The script architecture exists purely to allow you to run Lua programs larger than the GBA's 256Kb RAM limit. If you have any state that you need to preserve between scripts, you may use the poke function to stash variables in the `_IRAM` memory section (see Memory Regions below).

``` lua
-- main.lua
local a = 5000
local b = 2000

-- lots of code...

poke4(_IRAM, a)
poke4(_IRAM + 4, b)

next_script("other_file.lua")

```

* `fdog()`
Feed the engine's watchdog counter. You do not need to call this function if you are already calling the clear function. But if the engine does not receive `clear()` and `display()` calls for more than ten seconds, it assumes that a critical error occurred, and reloads the ROM. If you are running a complicated piece of code, perhaps when loading a level, you may want to feed the watchdog every so often. Or, if your program is not graphically intensive, and only rarely refreshes the screen, you may need to manually feed the watchdog.

# Memory Constraints

The Gameboy Advance has two memory sections: a small and fast internal work ram (IWRAM), and a much larger block of slightly slower external work ram (EWRAM). Most of the 32kB IWRAM is currently reserved for the engine, leaving 256kB for Lua code and data.

# Memory Regions

In addition to the memory used for Lua code and data, the engine provides access to a few other memory regions within the gba hardware, accessible via `peek()`, `peek4()`, `poke()`, and `poke4()`.

* `_IRAM`
An eight-thousand byte buffer of the fastest on-chip memory.

* `_SRAM`
A special memory region dedicated to save data. Writes can be quite slow, but the data will persist across restarts. Thirty-two kilobytes available. Under the hood, the engine restructures SRAM writes for you, so you do not need to worry about the single byte data bus (i.e. you may safely use poke4() with SRAM).


# Example

``` Lua

-- play some music from a 16 kHz signed 8-bit pcm wave file
music("my_music.raw", 0)

-- Fade the screen while we load a texture and fill the tile layer.
fade(1)

-- Load tileset from tile0.bmp into VRAM for the tile0 layer (layer 2).
txtr(2, "tile0.bmp")

-- Fill the tile0 map with some tiles.
for i = 0, 63 do
   for j = 0, 63 do
      tile(2, i, j, 1)
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
print(tostring(collectgarbage("count") * 1024), 3, 5)


-- enter main loop
main_loop(update, draw)


```

# Example Projects

* [Meteorain, by Dr. Ludos](https://github.com/drludos/meteorain-gba)


# Quirks

## Draw order

Calls to `spr()` will draw sprites with increasing depth (z-distance from the screen). Therefore, successive calls to `spr()` will place sprites behind previously drawn sprites. This may seem backwards at first, but we have a good reason for doing this. The Gameboy Advance only supports 128 sprites onscreen at a time. If you failed to properly keep track of your sprite count, and exceeded the limit, wouldn't you want the sprites further in the background to be hidden, rather than the nearer sprites?

## System font default colors

The overlay tile layer shares graphics memory with the system font. If you load an overlay, and find that the colors of your text now display unpredictably, this is becuause the overlay text will always, by default, use the second and third colors to appear in an overlay tilesheet as the foreground and background color. To calibrate the color of the system text, place an 8x8 pixel tile (like the one pictured below) in index zero of any overlay texture. You may set the top gray band to any arbitrary color in your tileset. Change the middle white band to the color that you want to use for the foreground color of the system text. Set the bottom black band to the background color for the system text.

<p align="center">
  <img src="index_tile.png"/>
</p>

