# BPCore-Engine

This repository includes parts of the BlindJump C++ engine, hacked together with a Lua interpreter, with the intention of allowing people to make gameboy games without needing to write C++ code or to use a compiler. The lua API for BPCore uses the simple APIs of fantasy consoles, like Pico8 or Tic80, as a model. In fact, many of the commands, like `spr()` and `btn()`, are almost the same.

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

# Status

This project is not very far along yet, mostly just a minimal outline for proof of concept. The ROM builds, of course, and supports all of the same features as the Blind Jump GBA engine. After building the ROM, you can use a script, `build.lua` to copy the contents of your lua scripts into the compiled ROM, and then the engine will search the ROM for the appended resource bundle, and execute the Lua code. So, the project works as a proof of concept, but until we have support for texture mapping from files in the resource bundle, you cannot really create a game with this engine yet. But at least we have a ROM skeleton, into which we can copy lua scripts, and we've got the Lua interpreter running on a gameboy, so that's a start!

But, anyway, all of the source code, from Blind Jump, exists for the following features, it just needs to be hooked into Lua:
* Link cable multiplayer
* Digital sound mixer
* Views and scrolling
* Unicode text engine
* Fades and other color palette effects
