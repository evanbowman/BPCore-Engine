

text("Hello, world!", 3, 3)


tilesheet(4, "spritesheet")


function main_loop(update, draw)
   while true do
      update(getdelta())
      clear()
      draw()
      display()
   end
end


function update(dt)
   if btnp(4) then
      tilesheet(2, "title_1_flattened")
   elseif btnp(5) then
      tilesheet(2, "title_2_flattened")
   end

   if btnp(6) then
      fade(0.5)
   elseif btnnp(6) then
      fade(0)
   end
end

i = 0
dir = 0
function draw()

   if dir == 0 then
      if i < 240 then
         i = i + 1
      else
         dir = 1
      end
   else
      if i > 0 then
         i = i - 1
      else
         dir = 0
      end
   end

   spr(15, i, 60)
end


text(tostring(collectgarbage("count") * 1024), 3, 5)


main_loop(update, draw)
