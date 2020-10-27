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
