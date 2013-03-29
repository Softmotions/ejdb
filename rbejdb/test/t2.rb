require "rbejdb"

db = EJDB.open("testdb", "rwct")
raise "Failed to open ejdb" unless db.is_open?
