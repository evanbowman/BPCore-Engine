---
---                              BUILD.lua
---
--- Build Script for BPCore. This script will copy code and data files into the
--- rom, in the format required by the engine.
---
---


if _VERSION ~= "Lua 5.3" then
   error("This script requires Lua 5.3")
end


bitmap = require("bitmap")


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


function extension(url)
   return url:match("^.+(%..+)$")
end


manifest = assert(loadfile("manifest.lua"))

application = manifest();
app_name = application["name"] .. ".gba"


-- The engine does not know how to parse .bmp files! Not that we cannot figure
-- out how to parse bmp files on a gameboy, that's easy. But needing to process
-- data before loading it into VRAM would significantly slow down texture
-- loading.
function convert_img(path)
   local img = bitmap.from_file(path)
   local w = img.width
   local h = img.height

   if h ~= 8 then
      -- TODO: support rectangular sizes
      error("tileset must be eight pixels high")
   end

   local color_index_count = 0
   local palette = {}

   local map_color = function(c15)
      local exists = palette[c15]
      if exists then
         return exists
      else
         palette[c15] = color_index_count
         result = color_index_count
         color_index_count = color_index_count + 1
         return result
      end
   end

   -- We're going to be outputting 4 bits per pixel, where the bits serve as an index
   local half = nil

   -- GBA hardware expects images to be meta-tiled, so that each 8x8 tile is
   -- effectively flattened, such that the first row of the 8x8 tile is followed
   -- immediately in memory by the next, and the next, for each row of the 8x8
   -- pixel tile.

   local img_data = ""

   local format_color = function(r, g, b)
      local red = bit32.rshift(r, 3)
      local green = bit32.rshift(g, 3)
      local blue = bit32.rshift(b, 3)

      return red + bit32.lshift(green, 5) + bit32.lshift(blue, 10)
   end

   -- Transparent color constant should have index 0
   map_color(format_color(255, 0, 255))

   outfile = io.open("debug", "wb")

   for block_x = 0, w - 1, 8 do
      for y = 0, h - 1 do
         for x = block_x, block_x + 8 - 1 do
            local r, g, b = img:get_pixel(x, y)

            local c15 = format_color(r, g, b)

            local index = map_color(c15)
            if half then
               half = bit32.bor(half, bit32.lshift(index, 4))
               img_data = img_data .. string.pack("<i1", half)
               half = nil
            else
               half = index
            end
         end
      end
   end

   outfile:write(img_data)
   outfile:close()

   palette_out = {}

   for k, v in pairs(palette) do
      palette_out[v] = k
   end

   palette_data = ""

   for i = 0, color_index_count - 1 do
      palette_data = palette_data .. string.pack("<i2", palette_out[i])
   end

   if color_index_count < 16 then
      for i = 0, 16 - color_index_count do
         palette_data = palette_data .. string.pack("<i2", 0)
      end
   end

   if color_index_count > 16 then
      error("Image must contain only 16 colors!")
   end

   return img_data, palette_data
end


-- Create the application bundle, by copying the engine ROM
cp("BPCoreEngine.gba", app_name)


bundle = io.open(app_name, "ab")


-- The engine uses this key to find the resource bundle within the rom.
bundle:write("core_filesys")


function bundle_resource(fname, data)
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


for _, fname in pairs(application["files"]) do

   local ext = extension(fname)

   if ext == ".bmp" then
      local data, palette = convert_img(fname)
      bundle_resource(fname, data)
      bundle_resource(fname .. ".pal", palette)
   else
      local data = contents(fname)
      bundle_resource(fname, data)
   end

end


-- write one empty header
for i = 0, 48 do
   bundle:write("\0")
end


bundle:close()
