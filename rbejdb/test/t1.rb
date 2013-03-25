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

puts "CONGRATULATIONS!!! EJDB tests has passed completely!"
