# BPCore-Engine

(**B**lind jum**P** **Core** Engine)

This repository includes parts of the [BlindJump](https://github.com/evanbowman/blind-jump-portable) C++ engine, hacked together with a Lua interpreter, with the intention of allowing people to make gameboy games without needing to write C++ code or to use a compiler. The lua API for BPCore uses the simple APIs of fantasy consoles, like Pico8 or Tic80, as a model. In fact, many of the commands, like `spr()` and `btn()`, are almost the same.


# Contents
<!--ts-->
   * [Contents](#contents)
   * [Architecture](#architecture)
   * [API](#api)
   * [Memory](#memory)
   * [Examples](#examples)
<!--te-->



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

### Button Presses

* `btn(num)`
Returns true if the button associated with `num` is pressed. Button ids: A-0, B-1, start-2, select-3, left-4, right-5, up-6, down-7, lbumper-8, rbumper-9.

* `btnp(num)`
Returns true if the button associated with `num` transitioned from unpressed to pressed.

* `btnnp(num)`
Returns true if the button associated with `num` transitioned from pressed to unpressed.

### Graphics

* `print(string, x, y, [foreground color hex], [background color hex])`
Render text to the overlay tile layer, using the system font. Supports Utf-8, although the engine does not include the whole universe of unicode glyphs, for practical reasons. BPCore ships with english alphanumeric characters, accented characters for Spanish and French, a selection of Japanese Katakana, the Russian alphabet, a couple of Scandanavian glyphs, and a sample of 2500 of the most common Chinese characters. By default, the string will use color indices 2 and 3 in the overlay layer's palette, but you can also use custom color ids. Note that x and y refer to tile layer coordinates in the overlay, not absolute screen pixels offsets. NOTE: Rendering text requires copying glyps into VRAM. The engine will use the first 80 tile slots in the overlay texture layer's ram for mapping glyphs into memory. You cannot display more than 80 unique text characters onscreen at a time.

* `txtr(layer, filename)`
Load image data from the resource bundle into VRAM. Layer refers to either the spritesheet, or one of the game's tile layers; layer0: overlay, layer1: map1, layer2: map0, layer4: spritesheet. The resource limits for the different layers vary, and will be enumerated here later. The overlay is drawn first, followed by sprites, followed by the two map layers. There is in fact another layer available, background layer3, which shares texture memory with layer2. layer3 and the overlay layer are 32x32 tiles in size, where each tile is 8x8 pixels in size. Map1 and Map0 are 64x64 tiles in size.

* `spr(index, x, y, [xflip], [yflip])`
Draw `index` from the spritesheet at screen pixel offset (`x`,`y`). Includes optional flipping flags.

* `tile(layer, x, y, [tile_num])`
Draw tile indicated by `tile_num` in tile layer `layer`, with coordinates `x` and `y`. Unlike `spr()`, tiles are persistent, and do not need to be redrawn for each frame. If called without `tile_num`, will instead return the current tile value at `x`,`y` in `layer`.

* `tilemap(filename, layer, width, height, [dest_x], [dest_y], [src_x], [src_y])`
Deserialize and load a tilemap from a file in the resource bundle. Currently, the file must be a CSV (with comma delimiters!) containing integer tile indices. `dest_x` and `dest_y` represent the top left coordinate in the tile `layer` into which to start loading the tile data. `src_x` and `src_y` represent the top left coordinates in the tilemap file to begin loading the data from. `width` and `height` represent the dimensions of the block of data that you want to load. The first four arguments must be specified, the latter arguments will be assumed to be zero if not supplied. This function will fail if filename does not exist, or if any of the width, height, src, or dest parameters would result in an out of bounds access. You could manually load tiles with the `tile()` function, `tilemap()` mainly exists to allow people to export levels from a map editor, and to speed up map loading. Added in version 2021.9.12.3.

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

* `rline()`
Returns the current raster line number. The GBA screen has 160 lines. You can use this function to implement a framerate limit, or for code profiling purposes. If the raster line advances past 160, you've spent too long updating data durnig the current frame and the game will lag. If you want to target 30fps, to give yourself more time for game updates:
```lua
-- game logic ...

if rline() > 160 then
    clear() -- framerate limit to max 30fps, by inserting another vsync.
end
clear() -- the regular clear()/display() calls
display()

```


### Entities

Entities share a lot in common with sprites, but with a few exceptions:
1) Entities have hitboxes and support collision checking.
2) The engine will automatically redraw entities for you (sprites will display _in front_ of entities).

All entity setters generally return the input entity as a result, so you can write `entpos(entspr(entity, 5), 1, 1)`, by chaining calls together. When called without additional arguments, the entity api functions act as getters (we want to provide getters, without using a bunch of gba memory by registering a duplicate set of functions).

* `ent()`
Create an entity. Max 128 allowed at a time.

* `del(entity, [parameter])`
Destroy an entity. The engine owns and manages all entities, the Lua garbage collector will not collect them. Call `del()` when you're done with an entity. If you pass an extra parameter: the following options are supported: parameter==0: no effect, the entity is not deleted, parameter==1: delete the entity when it finishes its animation.

* `entspr(entity, [sprite_id], [xflip], [yflip])`
Set an entity's sprite, with optional flipping flags. Similar to `spr()`, but for entities. Returns the input entity. When called without any of the last three arguments, returns an entity's sprite info:
```lua
entspr(entity, 5)                    -- set sprite id 5
entspr(entity, 10, true, false)      -- set sprite id with x-flip
local sprid, xflip, yflip = entspr(entity) -- retrieve sprite info
```

* `entpos(entity, [x], [y])`
Set an entity's position. Return the input entity. When called without the optional x,y arguments, returns an entity's position.
```lua
entpos(entity, 5, 4)    -- set entity position to 5,4
local x, y = entpos(entity)   -- retrieve entity x,y values.
```

* `entz(entity, [z])`
Assign an entity a Z value between 0 and 255, inclusive. The engine will sort entities by Z value when drawing them. Returns the input entity. When called without the Z argument, returns an entity's z value.
```lua
entz(entity, 5)          -- set z order to 5
local z = entz(entity)   -- retrieve z order
```

* `entag(entity, [integer])`
For tagging an entity with a numbered integer. If an integer argument is passed, the function will set the entity's tag and return the entity. If no extra arguments are passed, will return the entity's current tag. Tag should be within range [0, 65535].
```lua
entag(entity, 5000)      -- set entity tag to 5000
local t = entag(entity)  -- retrieve entity tag
```

* `enthb(entity, x_origin, y_origin, width, height)`
Set a hitbox for an entity. Hitboxes will be anchored to the center of an entity, after subtracting x_origin and y_origin. Hitbox width and height may not exceed 255. But the gba screen is only 240x160, so hopefully this won't be a problem :) Returns the input entity. By default, an entity will use a 16x16 hitbox, where the hitbox anchor is at 0,0 (the center of the 16x16 hitbox), to match the engine's sprite size.

* `entslots(entity, count)`
Allocates `count` slots for entity data members. You may store integer values in an entity's slot array.

* `entspd(entity, xspeed, yspeed)`
The engine will update your entity by xspeed, yspeed each frame.

* `entslot(entity, slot, [value])`
When called with two arguments, returns the value at `slot`. When called with an optional number value, assigns `value` to the specified slot, and returns the input entity. NOTE: like lua tables, entity slots use 1-based indexing. The function will raise a fatal error for out-of-bounds access.
```lua
e = ent()
entslots(e, 5)
entslot(e, 1, 22.1)
entslot(e, 1) -- 22.1
entslot(e, 0) -- fatal error!
entslot(e, 6) -- fatal error!
```

* `ents()`
Get a table of all entities registered with the engine. Normally, you should not need to call this function. Entities should be considered a resource belonging to the engine, and the Lua environment will not garbage collect unused entities. If you're switching scripts with next_script(), you may sometimes need to ask the engine for its list of entities. Otherwise, you should do your best to keep track of entities. This function allocates a table, and you should not rely on calling it every frame.

* `entanim(entity, start_keyframe, length, rate)`
Animate an entity. The engine will cycle through keyframes, until reaching start_keyframe + length. The engine will advance one keyframe for every `rate` display() calls. As it's very common to create animated effects and then delete them when finished, the `del()` function allows you to delete an entity in the future, when it finishes its animation.


#### Collisions

The engine offers a few different collision functions for entities:

* `ecole(e1, e2)`
(entity-collide-entity)
Checks collisions between two entities, returns true if a collision exists.

* `ecolt(e1, tag)`
(entity-collide-tag)
Returns an array of all entities tagged with tag that collide with with entity e1. See `entag()` for entity tagging. NOTE: will return at most 16 colliding entities.

* `ecolm(e1, layer, [solid_tile_ids])`
(entity-collide-tile-map)
Unimplemented, planned for a future release!


### RAM Read/Write

NOTE: Only _SRAM and _IRAM regions are writable (see [Memory](#memory) below).

* `poke(address, byte)`
Set byte at a writable address.

* `poke4(address, word)`
Set word at a writable address.

* `peek(address)`
Returns byte value at any address.

* `peek4(address)`
Returns word value at any address.

* `memput(address, string)`
Copy contents of string to a writable address.

* `memget(address, count)`
Load a string of count bytes starting from an address.

* `file(name)`
Returns a pointer,length to any file in the resource bundle. The data can then be read with the peek/peek4 functions. You cannot write to files, as they reside in ROM, and are therefore, by definition, read-only.

``` lua
ptr, len = file("main.lua")
print(memget(ptr, 10), 1, 1) -- print the first 10 chars of this very script.
print(string.char(peek(ptr + 3)), 1, 3) -- print the fourth byte of this file
```



### Math Utilities

* `dirv(x1, y1, x2, y2)`
Computes a unit vector representing the direction between x1,y1 and x2,y2. Returns two results represeting the x and y of the unit vector.

* `rotv(x, y, r)`
Rotate an x,y vector by r degrees.

### Sound

* `music(source_file, offset)`
Play mono 16kHz signed 8bit PCM audio from the given source file string. All music loops, and you may specify a microsecond offset into the music file with the `offset` parameter.

* `sound(source_file, priority)`
Play mono 16kHz signed 8bit PCM audio from the given source file string. Unlike the music, sounds do not loop. The engine can only render four audio channels at a time--3 for sound effects, and one for the music. If you already have three sounds playing, the sound effect with the lowest priority will be evicted if the sound that you are requesting has a higher priority.

### Program Structure

* `next_script(name)`
Execute script `name` when the current script runs to completion. Due to memory constraints (the GBA has limited RAM), you may need to structure your program as a series of isolated scripts. Each script is completely independent, i.e. scripts start with a clean slate when they begin running. Therefore, Lua global variables may not be shared between lua scripts, so you will need to write any persistent data into an unused section of GBA RAM. While swapping scripts will erase any existing Lua code or Lua variables from RAM, starting a new script does not otherwise impact the state of the BPCore engine, so, for example, any tiles that you created in the current script, will be unchanged when moving to the next script. The script architecture exists purely to allow you to run Lua programs larger than the GBA's 256Kb RAM limit. If you have any state that you need to preserve between scripts, you may use the poke function to stash variables in the `_IRAM` memory section (see Memory Regions below).

``` lua
-- main.lua
local a = 5000
local b = 2000

-- lots of code...

poke4(_IRAM, a)
poke4(_IRAM + 4, b)

next_script("other_file.lua")

```

### Serial I/O

You can use the engine's asynchronous I/O library to send data to another GBA device, using the GBA's multiplayer link mode. Currently, the engine only supports two connected devices, with plans to support four devices in the future.

BPCore's implementation of network I/O does not guarantee that messages will be received in-order, or even received at all. Each device maintains a 64-packet receive queue, as well as a 32-packet send queue. Overflowing either the send queue in the sender, or the receive queue in the receiver, will result in packet loss. That said, I've used this network implementation in several GBA games; Blind Jump, Skyland, etc., and I've never had any problems with packet loss. If you limit your send() calls to a few packets per frame, you will never see any message loss.

* `connect(timeout_seconds)`
Attempt to connect to another GBA device via the link port. Return true upon success, false upon failure. The function will automatically fail after timeout_seconds, if no other device successfully connected. `connect` is the only blocking call in the Serial I/O library, all other network functions use asynchronous I/O.

* `send(message_string)`
Send a message to another device. The `send` function queues the input message string, and returns immediately with a success/failure code. The function may fail if you exhaust the outgoing message queue, or if the message_string exceeds the engine's eleven-byte-per-message size limit. NOTE: you don't, of course, need to send a human-readable string, it just happens that a string is the most flexible option for accepting either a human readable character string, or binary encoded data.

* `recv()`
Polls the receive queue for message strings. Returns a message, if one is available, or returns nil, if there are no messages in the queue. The returned messages will be prefixed with a single character device id representing the sender, e.g. if you send("hi") on one device, you will receive "1hi" if the message originated in the player1 console, or "2hi" if the player2 console sent the message, etc.. As the engine only supports two-player connections, you may be wondering why I bother to include a device id prefix at all: in case I add support for 4 player connections, you may care where the message originated, and I don't want to break backwards compatibility, so messages include a device id, even if it's not especially useful yet. If you want to drain the receive queue, simply run `recv` in a loop:
``` lua
local pkt = recv()
while pkt do
   -- do something with pkt
   pkt = recv()
end
```

* `disconnect()`
Close the multiplayer session. During the diconnect process, the engine will send "$disconnect!" to the other device, substituting $ with the device id. Calls to connect() will implicitly call disconnect() if there is already an active connection.

### Advanced Serial I/O

Admittedly, packing/unpacking binary data from Lua strings can have a performance impact in tight loops. As of version 21.9.13.1, the engine includes two extra send/recv functions, `send_iram()` and `recv_iram()`, allowing you to read plain bytes out of the packets with `peek()` and `poke()`:

* `send_iram(iram_address)`:
Send raw 11 byte packet starting at address iram_address. Return true upon success, false upon falure.

* `recv_iram(iram_address)`:
If a packet exists in the receive queue, map that 12 byte packet into IRAM, starting at iram_address. Like with `recv()`, the first byte of the packet represents the originating device, followed by the 11 bytes of data. Returns false if the receive queue was empty, true otherwise.

Example: advanced serial I/O usage, sends coordinates back and forth between devices:
``` lua
while not btnp(0) do
   clear()
   display()
   -- wait on a button press, then connect
end
connect(10)

local x = 0
local y = 0
local ox = 0
local oy = 0

while true do

   poke4(_IRAM, x)
   poke4(_IRAM + 4, y)
   send_iram(_IRAM)

   local got_msg = recv_iram(_IRAM)
   while got_msg do
      sender = peek(_IRAM) -- message originator
      ox = peek4(_IRAM + 1) -- x, y from other device
      oy = peek4(_IRAM + 5)
      got_msg = recv_iram(_IRAM)
   end

   clear()
   display()
end
```


### System

* `sleep(frames)`
Sleep the game for N frames.

* `delta()`
Returns time since the last delta call in microseconds. Because games written in Lua may push the Gameboy CPU to its limits, games may not run at a steady framerate. You can either carefully fine-tune your game to run at a specific framerate, or, scale game updates based on the frame delta.

* `fdog()`
Feed the engine's watchdog counter. You do not need to call this function if you are already calling the clear function. But if the engine does not receive `clear()` and `display()` calls for more than ten seconds, it assumes that a critical error occurred, and reloads the ROM. If you are running a complicated piece of code, perhaps when loading a level, you may want to feed the watchdog every so often. Or, if your program is not graphically intensive, and only rarely refreshes the screen, you may need to manually feed the watchdog.

* `startup_time()`
If the cartridge hardware includes a realtime clock (RTC), `startup_time()` returns a table representing the value of the RTC when the engine booted up. There is currently no way in the engine to overwrite the value of the RTC chip, so if you want to allow a user to overwrite the clock, you can achieve the same behavior by storing user supplied values in SRAM, and adding them as offsets to the result of startup_time(). You can keep track of the elapsed time since startup by aggregating the results of the `delta()` API calls.

Result table format (string keys, integer values)
```lua
{
   year = 21,
   month = 8,
   day = 12,
   hour = 11,
   minute = 6,
   second = 3
}
```

(Added in version 2021.9.12.1)

* `_BP_VERSION`
Starting with version 2021.9.12.0, the engine stores a version string in the `_BP_VERSION` variable.

* `dofile(filename)`
Evaluate code in a separate lua file. Putting all your code in one file may use less memory, but people may want to organize projects as separate files, especially if for shared code that you want to reference in different script contexts evaluated using `next_script()`. Unlike the standard lua dofile, _this function does not return a value_.


### Reserved words

The function `syscall`, as well as the variable `util`, should be considered reserved for future use. Do not use these variable names, if you want to seamlessly migrate to new versions of the engine.


# Memory

## Memory Constraints

The Gameboy Advance has two memory sections: a small and fast internal work ram (IWRAM), and a much larger block of slightly slower external work ram (EWRAM). Most of the 32kB IWRAM is currently reserved for the engine, leaving 256kB for Lua code and data.

## Memory Regions

In addition to the memory used for Lua code and data, the engine provides access to a few other memory regions within the gba hardware, accessible via `peek()`, `peek4()`, `poke()`, and `poke4()`.

* `_IRAM`
An eight-thousand byte buffer of the fastest on-chip memory.

* `_SRAM`
A special memory region dedicated to save data. Writes can be quite slow, but the data will persist across restarts. Thirty-two kilobytes available. Under the hood, the engine restructures SRAM writes for you, so you do not need to worry about the single byte data bus (i.e. you may safely use poke4() with SRAM).


# Examples

## Example Projects

For a project template, see [here](https://github.com/evanbowman/bpcore-project-template/tree/master)

* [Meteorain, by Dr. Ludos](https://github.com/drludos/meteorain-gba)
* [HyperWing, a demo boss-rush shmup](https://github.com/evanbowman/hyperwing)
* [Unicode eBook Reader](https://github.com/evanbowman/bpcore-gba-book-reader)
* [Simple Image Demo](https://github.com/evanbowman/bpcore-simple-image-demo)
* [Fullscreen Image Demo](https://github.com/evanbowman/bpcore-fullscreen-image-demo)

## A tiny demo
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



# Quirks

## Draw order

Calls to `spr()` will draw sprites with increasing depth (z-distance from the screen). Therefore, successive calls to `spr()` will place sprites behind previously drawn sprites. This may seem backwards at first, but we have a good reason for doing this. The Gameboy Advance only supports 128 sprites onscreen at a time. If you failed to properly keep track of your sprite count, and exceeded the limit, wouldn't you want the sprites further in the background to be hidden, rather than the nearer sprites?

## System font default colors

The overlay tile layer shares graphics memory with the system font. If you load an overlay, and find that the colors of your text now display unpredictably, this is becuause the overlay text will always, by default, use the second and third colors to appear in an overlay tilesheet as the foreground and background color. To calibrate the color of the system text, place an 8x8 pixel tile (like the one pictured below) in index zero of any overlay texture. You may set the top gray band to any arbitrary color in your tileset. Change the middle white band to the color that you want to use for the foreground color of the system text. Set the bottom black band to the background color for the system text.

<p align="center">
  <img src="index_tile.png"/>
</p>

## fade() and custom font colors

`fade()` does not apply to colored text, i.e. if you passed custom color hex values to the print() function. Supporting this would be practically unrealistic given the cpu frequency on a gameboy advance (we cannot realistically linearly interpolate between 256 arbitrary colors within a reasonable amount of time). Regular text, using the default overlay palette, can be faded. 

# Future Work

The BlindJump source code has tons of other features that I'd like to eventually add to the Lua API. In the future, I plan to add:
* Gameboy Player Rumble
* Loading data from the Tiled map editor
* UART
* Hitboxes and builtin collision checking
