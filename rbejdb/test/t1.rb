require "rbejdb"

ejdb = EJDB.open("zoo", EJDB::DEFAULT_OPEN_MODE)

raise "Failed to open ejdb" unless ejdb.is_open?

ejdb.dropCollection("parrots", true)
ejdb.dropCollection("cows", true)

ejdb.ensureCollection("parrots")
ejdb.ensureCollection("parrots", {"large" => true, "records" => 200000})

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

ejdb.save("cows", :name => "moo")


ejdb.find("parrots", {:name => "Cacadoo"}).each { |res|
  puts res.inspect
}

ejdb.find("parrots", {:name => {"$in" => ["Mamadoo", "Sauron"]}}).each { |res|
  puts res.inspect
}

puts ejdb.find("cows", {}).to_a.inspect

raise "Error querying cows"  unless ejdb.find("cows", :name => "moo").any? {|res| res["name"] == 'moo'}

ejdb.close

raise "Failed to close ejdb" unless !ejdb.is_open?

puts "CONGRATULATIONS!!! EJDB tests have passed completely!"
