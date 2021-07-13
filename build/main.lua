
fade(1)

txtr(0, "overlay.bmp")

print("Hello, world!", 3, 3)

txtr(2, "tile0.bmp")


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


function update(dt)
   if btnp(0) then
      if connect() then
         print("connected!", 3, 12)
      else
         print("connection failed!", 3, 12)
      end
   end
end


function draw()
end


print(tostring(collectgarbage("count") * 1024), 3, 5)


main_loop(update, draw)
