
fade(1)

txtr(0, "overlay.bmp")

tile(0, 9, 9, 3)
tile(0, 8, 9, 27)
tile(0, 7, 9, 16)
tile(0, 6, 9, tile(0, 9, 9))

print(_BP_VERSION, 3, 3)

--txtr(2, "tile0.bmp")


txtr(2, "test.bmp")

function draw_img(layer, x, y, w, h)
   local t = 0
   for yy = 0, h - 1 do
      for xx = 0, w - 1 do
         tile(layer, x + xx, y + yy, t)
         t = t + 1
      end
   end
end

draw_img(2, 0, 0, 30, 4)


fade(0)


txtr(4, "spritesheet.bmp")


music("music_as_colorful_as_ever.raw", 0)


function main_loop(update, draw)
   while true do
      update(delta())
      clear()
      draw()
      display()
   end
end


local x = 0
local y = 0

local i = 0


function update(dt)

   if btnp(0) then
      fade(0.5)
      i = (i + 1) % 250
   elseif btnnp(0) then
      fade(0)
   end

   if btn(5) then
      x = x + 2
   elseif btn(4) then
      x = x - 2
   end

   if btn(6) then
      y = y - 2
   elseif btn(7) then
      y = y + 2
   end

   -- camera(x, y)
end


function draw()
   spr(0, x, y)
   spr(255, 30, 30)
end


print(tostring(collectgarbage("count") * 1024), 3, 5)


poke4(_IRAM, 42)

print(tostring(peek4(_IRAM)), 3, 6)


main_loop(update, draw)
