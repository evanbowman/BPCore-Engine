#pragma once

#include <stddef.h>

// This header provides the bare minimum needed for writing extensions to the
// engine. I could expose more engine internals if people request access to
// various things.


#ifndef LUA_32BITS
#define LUA_32BITS
#endif

#ifndef LUA_COMPAT_5_3
#define LUA_COMPAT_5_3
#endif

#include <lua/lauxlib.h>
#include <lua/lualib.h>


#ifdef __cplusplus
extern "C" {
#endif


// NOTE: Do not call libc malloc and free. The engine uses its own version, and
// you should call bpcore_malloc instead. The engine uses all avaliable ram, and
// if you try to call malloc from the C standard library, it will just fail.
// Besides, the malloc in newlib isn't great anyway.
void* bpcore_malloc(size_t size);
void bpcore_free(void* ptr);


// You must implement this function, to register any lua functions you'd like to
// add to the engine. Other than registering functions, your extension entry
// function should be stateless and reentrant. The engine may call
// bpcore_extension_main multiple times throughout the execution of a program,
// so you should not use this function to initialize global data.
void bpcore_extension_main(lua_State* L);



#ifdef __cplusplus
}
#endif
