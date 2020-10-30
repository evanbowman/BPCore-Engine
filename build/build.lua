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


-- Bitmap library inlined here for user-friendliness. We do not want to require
-- users to keep track of more than this script, the base rom, and their
-- manifest file.
bitmap =
(function ()
    local Bitmap = {}

    local function read_word(data, offset)
       return data:byte(offset+1)*256 + data:byte(offset)
    end

    local function read_dword(data, offset)
       return read_word(data, offset+2)*65536 + read_word(data, offset)
    end

    local function pack(...)
       return {...}
    end

    function Bitmap.from_string(data)
       if not read_dword(data, 1) == 0x4D42 then -- Bitmap "magic" header
          return nil, "Bitmap magic not found"
       elseif read_word(data, 29) ~= 24 then -- Bits per pixel
          return nil, "Only 24bpp bitmaps supported"
       elseif read_dword(data, 31) ~= 0 then -- Compression
          return nil, "Only uncompressed bitmaps supported"
       end

       local bmp = {} -- We'll return this to the user

       bmp.data = data
       bmp.pixel_offset = read_word(data, 11);
       bmp.width = read_dword(data, 19);
       bmp.height = read_dword(data, 23);

       function bmp:get_pixel(x,y)
          if (x < 0) or (x > self.width) or (y < 0) or (y > self.height) then
             return nil, "Out of bounds"
          end
          local index = self.pixel_offset + (self.height - y - 1)*3*self.width + x*3
          local b = self.data:byte(index+1)
          local g = self.data:byte(index+2)
          local r = self.data:byte(index+3)
          return r,g,b
       end

       function bmp:write_to_file(path)
          local file io.open(path, "wb")
          if not file then
             return nil, "Can't open file"
          end
          file:write(bmp.data)
          file:close()
          return #bmp.data
       end

       function bmp:to_string()
          return data
       end

       function bmp:set_pixel(x,y,r,g,b)
          if (x < 0) or (x > self.width) or
             (y < 0) or (y > self.height) or
             (r<0) or (r>255) or
             (g<0) or (g>255) or
          (b<0) or (b>255) then
             return nil, "Out of bounds"
          end
          local index = self.pixel_offset + (self.width*3)*y + x
          local r = self.data:byte(index)
          local g = self.data:byte(index+1)
          local b = self.data:byte(index+2)
          local start = self.data:sub(1,index-1)
          local mid = string.char(r,g,b)
          local stop = self.data:sub(index+1, index+3)
          self.data = start .. mid .. stop
          return true
       end

       function bmp:get_rect(data, x, y, w, h)
          local data = assert(bitmap.data)
          local rect = {}
          for cy=y, y+h do
             local line = {}
             local empty = true
             for cx=x,x+w do
                if not ((x < 0) or (x > self.width) or (y < 0) or (y > self.height)) then
                   local index = pixel_offset + (self.width*3)*y + x
                   local r,g,b = self.data:byte(index, index+2)
                   if r and g and b then
                      line[#line + 1] = {r,g,b}
                      empty = false
                   end
                end
             end
             if empty then
                setmetatable(line, {__index = function(t, k) if k == "empty" then return true end end})
             end
             rect[#rect+1] = line
          end
          return rect
       end

       return bmp

    end

    function Bitmap.from_file(path)
       local file = io.open(path, "rb")
       if not file then
          return nil, "Can't open file!"
       end
       local content = file:read("*a")
       file:close()
       return Bitmap.from_string(content)
    end

    return Bitmap
end)()


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


local format_color = function(r, g, b)
   local red = bit32.rshift(r, 3)
   local green = bit32.rshift(g, 3)
   local blue = bit32.rshift(b, 3)

   return red + bit32.lshift(green, 5) + bit32.lshift(blue, 10)
end


function write_tile(start_x, start_y, img, map_color)
   -- GBA hardware expects images to be meta-tiled, so that each 8x8 tile is
   -- effectively flattened, such that the first row of the 8x8 tile is
   -- followed immediately in memory by the next, and the next, for each row
   -- of the 8x8 pixel tile.

   local result = ""

   -- We're going to be outputting 4 bits per pixel, where the bits serve as an
   -- index into a palette table (i.e. we're dealing with 4-bit indexed color).
   local half = nil

   for y = start_y, start_y + 8 - 1 do
      for x = start_x, start_x + 8 - 1 do
         local r, g, b = img:get_pixel(x, y)

         local c15 = format_color(r, g, b)

         local index = map_color(c15)
         if half then
            half = bit32.bor(half, bit32.lshift(index, 4))
            result = result .. string.pack("<I1", half)
            half = nil
         else
            half = index
         end
      end
   end

   return result
end


function make_color_mapper(palette)

   local color_index_count = 0

   return function(c15_hex)
      local exists = palette[c15_hex]
      if exists then
         return exists
      else
         palette[c15_hex] = color_index_count
         result = color_index_count
         color_index_count = color_index_count + 1
         return result
      end
   end
end


function write_palette(palette, source)
   local palette_out = {}
   local result = ""

   local id_max = 0

   for k, v in pairs(palette) do
      if v > id_max then
         id_max = v
      end
      palette_out[v] = k
   end

   if id_max >= 16 then
      error("image " .. source .. " has too many colors! (Expected: 16)")
   end

   for i = 0, id_max do
      result = result .. string.pack("<I2", palette_out[i])
   end

   if id_max ~= 15 then
      for i = id_max + 1, 15 do
         result = result .. string.pack("<I2", 0)
      end
   end

   return result
end


-- The engine does not know how to parse .bmp files! Not that we cannot figure
-- out how to parse bmp files on a gameboy, that's easy. But needing to process
-- data before loading it into VRAM would significantly slow down texture
-- loading.
function convert_tileset(path)
   local img = bitmap.from_file(path)
   local w = img.width
   local h = img.height

   if h ~= 8 then
      -- TODO: support rectangular sizes
      error("tileset must be eight pixels high")
   end

   local palette = {}

   map_color = make_color_mapper(palette)

   -- Transparent color constant should have index 0
   map_color(format_color(255, 0, 255))

   local img_data = ""

   for block_x = 0, w - 1, 8 do
      img_data = img_data .. write_tile(block_x, 0, img, map_color)
   end

   return img_data, write_palette(palette, path)
end


-- The engine uses 8x8 pixel tiles, and 16x16 pixel sprites, so sprite data
-- needs to be processed differently (meta-tiled 2x2)
function convert_spritesheet(path)
   local img = bitmap.from_file(path)
   local w = img.width
   local h = img.height

   if h ~= 16 then
      -- TODO: support rectangular sizes
      error("spritesheet must be sixteen pixels high")
   end

   local palette = {}

   map_color = make_color_mapper(palette)

   -- Transparent color constant should have index 0
   map_color(format_color(255, 0, 255))

   local img_data = ""

   for meta_x = 0, w - 1, 16 do
      for block_y = 0, h - 1, 8 do
         for block_x = meta_x, meta_x + 16 - 1, 8 do
            local tile_data = write_tile(block_x, block_y, img, map_color)
            img_data = img_data .. tile_data
         end
      end
   end

   return img_data, write_palette(palette, path)
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
   -- padding (for word alignment)

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

   -- BPCore requries subsequent data to be word-aligned.
   --- Note: +1 for the null byte.
   for i = 0, ((datalen + 1) % 4) - 1 do
      bundle:write("\0")
   end

end


for _, fname in pairs(application["tilesets"]) do
   if extension(fname) ~= ".bmp" then
      error("tilesets should be in a .bmp format!")
   end

   local data, palette = convert_tileset(fname)
   bundle_resource(fname, data)
   bundle_resource(fname .. ".pal", palette)
end


for _, fname in pairs(application["spritesheets"]) do
   if extension(fname) ~= ".bmp" then
      error("spritesheets should be in a .bmp format!")
   end

   local data, palette = convert_spritesheet(fname)
   bundle_resource(fname, data)
   bundle_resource(fname .. ".pal", palette)
end


for _, fname in pairs(application["scripts"]) do
   bundle_resource(fname, contents(fname))
end


for _, fname in pairs(application["audio"]) do
   bundle_resource(fname, contents(fname))
end


for _, fname in pairs(application["misc"]) do
   bundle_resource(fname, contents(fname))
end


-- write one empty header
for i = 0, 48 do
   bundle:write("\0")
end


bundle:close()
