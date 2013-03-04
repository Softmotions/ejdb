--
-- print the library path of Lua
--

for str in string.gmatch(package.cpath, "[^;]+") do
   if string.match(str, "^/[%w%._-]") and string.match(str, "/%?%.%w+$") then
      str = string.gsub(str, "/[^/]+$", "")
      if os.execute("test -d " .. str) == 0 then
         print(str)
         break
      end
   end
end
