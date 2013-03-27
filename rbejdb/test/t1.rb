require "rbejdb"

ejdb = EJDB.new()
ejdb.open("zoo", EJDB::DEFAULT_OPEN_MODE)

raise "Failed to open ejdb" unless ejdb.is_open?

class Parrot
  def initialize(name, size)
    @name = name
    @size = size
  end
end

parrot1 = Parrot.new("Cacadoo", 12)
ejdb.save("parrots", parrot1)

parrot2 = {:name => "Mamadoo", :size => 666, "likes" => ["green color", "night", ["toys", "joys"], parrot1]}
ejdb.save("parrots", parrot2)


ejdb.find("parrots", {:name => "Cacadoo"}) { |res|
  puts res.inspect
}

ejdb.find("parrots", {:name => "Mamadoo"}) { |res|
  puts res.inspect
}

puts "CONGRATULATIONS!!! EJDB tests have passed completely!"
