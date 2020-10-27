

text("Hello, world!", 3, 3)


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
end


function draw()

end


main_loop(update, draw)
