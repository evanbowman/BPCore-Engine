# BPCore-Engine

This repository includes parts of the BlindJump C++ engine, hacked together with a Lua interpreter, with the intention of allowing people to make gameboy games without needing to write C++ code or to use a compiler. The lua API for BPCore uses the simple APIs of fantasy consoles, like Pico8 or Tic80, as a model. In fact, many of the commands, like `spr()` and `btn()`, are almost the same.

# Status

This project is not very far along yet, mostly just a minimal outline for proof of concept. The ROM builds, of course, and supports all of the same features as the Blind Jump GBA engine. After building the ROM, you can use a script, `build.lua` to copy the contents of your lua scripts into the compiled ROM, and then the engine will search the ROM for the appended resource bundle, and execute the Lua code. So, the project works as a proof of concept, but until we have support for texture mapping from files in the resource bundle, you cannot really create a game with this engine yet. But at least we have a ROM skeleton, into which we can copy lua scripts, and we've got the Lua interpreter running on a gameboy, so that's a start!

But, anyway, all of the source code, from Blind Jump, exists for the following features, it just needs to be hooked into Lua:
* Link cable multiplayer
* Digital sound mixer
* Views and scrolling
* Unicode text engine
* Fades and other color palette effects
