require "rbejdb"
require 'test/unit'


$now = Time.now
$jb = EJDB.open("tdbt2", EJDB::JBOWRITER | EJDB::JBOCREAT | EJDB::JBOTRUNC)

class EJDBTestUnit < Test::Unit::TestCase
  def test_save_load
      assert $jb.is_open?

      parrot1 = {
          "name" => "Grenny",
          "type" => "African Grey",
          "male" => true,
          "age" => 1,
          "birthdate" => $now,
          "likes" => ["green color", "night", "toys"],
          "extra1" => nil
      }
      parrot2 = {
          "name" => "Bounty",
          "type" => "Cockatoo",
          "male" => false,
          "age" => 15,
          "birthdate" => $now,
          "likes" => ["sugar cane"],
          "extra1" => nil
      }

      oids = $jb.save("parrots", parrot1, nil, parrot2)
      assert_not_nil oids
      assert_equal(oids.length, 3)
      assert_equal(parrot1["_id"], oids[0])
      assert_nil oids[1]
      assert_equal(parrot2["_id"], oids[2])

      obj = $jb.load("parrots", parrot2["_id"])
      assert_not_nil obj
      assert_equal(obj["_id"], parrot2["_id"])
      assert_equal(obj["name"], "Bounty")
  end
end
