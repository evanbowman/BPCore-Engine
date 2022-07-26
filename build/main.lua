
fade(1)

txtr(0, "overlay.bmp")

tile(0, 9, 9, 3)
tile(0, 8, 9, 27)
tile(0, 7, 9, 16)
tile(0, 6, 9, tile(0, 9, 9))

print(_BP_VERSION, 3, 3)

txtr(2, "tile0.bmp")

player = ent()
entspr(player, 0)

enemy = ent()
entpos(entspr(enemy, 255), 32, 32)
entag(enemy, 3)

entag(entpos(entspr(ent(), 255), 32, 48), 3)



for i = 15, 45 do
   for j = 15, 45 do
      tile(2, i, j, 1)
   end
end


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

   camera(x, y)
   entpos(player, x, y)

   local cols = ecolt(player, 3)
   if cols then
      local cnt = 0
      for _, obj in ipairs(cols) do
         cnt = cnt + 1
      end
      print("collisions:" .. cnt, 1, 14)
   else
      print("collisions:0", 1, 14)
   end

   cols = ecolm(player, 2, _IRAM)
   if cols then
      print("tc: " .. cols)
   end
end


function draw()
   -- ...
end


print(tostring(collectgarbage("count") * 1024), 3, 5)


poke4(_IRAM, 42)

print(tostring(peek4(_IRAM)), 3, 6)


main_loop(update, draw)
