---
---                              BUILD.lua
---
--- Build Script for BPCore. This script will copy code and data files into the
--- rom, in the format required by the engine.
---
---


function contents(filename)
   infile = io.open(filename, "rb")
   instr = infile:read("*a")
   infile:close()
   return instr
end


function cp(from, to)

   instr = contents(from)

   outfile = io.open(to, "wb")
   outfile:write(instr)
   outfile:close()
end


manifest = assert(loadfile("manifest.lua"))

application = manifest();
app_name = application["name"] .. ".gba"


-- Create the application bundle, by copying the engine ROM
cp("BPCoreEngine.gba", app_name)


bundle = io.open(app_name, "ab")


-- The engine uses this key to find the resource bundle within the rom.
bundle:write("core_filesys")


for _, fname in pairs(application["files"]) do
   data = contents(fname)

   -- Bundle Resource Header:
   --
   -- char name[32]
   -- char size[16]
   -- contents[...]
   -- null byte

   bundle:write(fname)

   for i = 0, 31 - string.len(fname) do
      bundle:write("\0")
   end

   local datalen = string.len(data)
   local lenstr = tostring(datalen)
   bundle:write(lenstr)

   for i = 0, 15 - string.len(lenstr) do
      bundle:write("\0")
   end

   bundle:write(data)

   -- BPCore requires data to be null-terminated
   bundle:write("\0")
end


-- write one empty header
for i = 0, 48 do
   bundle:write("\0")
end


bundle:close()
