require "rbejdb"

ejdb = EJDB.new()
ejdb.open("zoo", EJDB::DEFAULT_OPEN_MODE)

raise "Failed to open ejdb" unless ejdb.is_open?

puts "CONGRATULATIONS!!! EJDB tests has passed completely!"