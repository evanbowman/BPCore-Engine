
fade(1)

txtr(0, "overlay.bmp")

tile(0, 111, 9, 9)
tile(0, 110, 8, 9)
tile(0, 91, 7, 9)
tile(0, 1, 6, 9)

text("Hello, world!", 3, 3)

txtr(2, "tile0.bmp")

for i = 0, 63 do
   for j = 0, 63 do
      tile(2, 1, i, j)
   end
end


fade(0)


txtr(4, "spritesheet")


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
local dir = 0

function update(dt)

   if btnp(6) then
      fade(0.5)
   elseif btnnp(6) then
      fade(0)
   end

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
   spr(15, x, 60)
end


text(tostring(collectgarbage("count") * 1024), 3, 5)


main_loop(update, draw)
