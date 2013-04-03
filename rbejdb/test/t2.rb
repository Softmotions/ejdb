require "rbejdb"
require 'test/unit'

$now = Time.now
$jb = EJDB.open("tdbt2", EJDB::JBOWRITER | EJDB::JBOCREAT | EJDB::JBOTRUNC)


class EJDBTestUnit < Test::Unit::TestCase

  def test_ejdb1_save_load
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
    assert_equal(3, oids.length)
    assert_equal(parrot1["_id"], oids[0])
    assert_nil oids[1]
    assert_equal(parrot2["_id"], oids[2])

    obj = $jb.load("parrots", parrot2["_id"])
    assert_not_nil obj
    assert_equal(parrot2["_id"], obj["_id"])
    assert_equal("Bounty", obj["name"])
  end


  def test_ejdb2_query1
    assert_not_nil $jb
    assert $jb.is_open?

    results = $jb.find("parrots", {})

    assert_not_nil results

    assert_equal(2, results.to_a.length)
    assert_equal(2, results.to_a.length) #checking second call gets same result

    results.each { |rv|
      assert rv
      assert rv["_id"].is_a? String
      assert rv["name"].is_a? String
      assert rv["age"].is_a? Numeric
      assert rv["birthdate"] != nil && rv["birthdate"].is_a?(Object)
      assert rv["male"] == true || rv["male"] == false
      assert_nil rv["extra1"]
      assert rv["likes"] != nil && rv["likes"].is_a?(Array)
      assert rv["likes"].length > 0
    }

    results = $jb.find("parrots", {})
    assert_not_nil results
    assert_equal(2, results.to_a.length)


    #count mode
    count = $jb.find("parrots", {}, :onlycount => true)
    assert count.is_a? Numeric
    assert_equal(2, count)

    #findOne
    obj = $jb.find_one("parrots")
    assert_not_nil obj
    assert obj["name"].is_a? String
    assert obj["age"].is_a? Numeric

    #empty query
    results = $jb.find("parrots")
    assert_not_nil results
    assert_equal(2, results.to_a.length)
  end


  def test_ejdb3_test_query2
    assert_not_nil $jb
    assert $jb.is_open?

    results = $jb.find("parrots", {:name => /(grenny|bounty)/i}, {:orderby => {:name => 1}})

    assert_not_nil results
    assert_equal(2, results.to_a.length)

    results.each_with_index { |rv, index|
      if index == 0
        assert_equal("Bounty", rv["name"])
        assert_equal("Cockatoo", rv["type"])
        assert_equal(false, rv["male"])
        assert_equal(15, rv["age"])
        assert_equal($now.inspect, rv["birthdate"].inspect)
        assert_equal("sugar cane", rv["likes"].join(","))
      end
    }
  end


  def test_ejdb4_test_query3

    assert_not_nil $jb
    assert $jb.is_open?

    results = $jb.find("parrots", {}, [{:name => "Grenny"}, {:name => "Bounty"}], {:orderby => {:name => 1}})

    assert_not_nil results
    assert_equal(2, results.to_a.length)

    results.each_with_index { |rv, index|
      if index == 1
        assert_equal("Grenny", rv["name"])
        assert_equal("African Grey", rv["type"])
        assert_equal(true, rv["male"])
        assert_equal(1, rv["age"])
        assert_equal($now.inspect, rv["birthdate"].inspect)
        assert_equal("green color,night,toys", rv["likes"].join(","))
      end
    }


    assert_equal(2, results.to_a.length)

    # testing force close
    results.close

    assert_raise(RuntimeError) {
      results.to_a.length
    }
  end

end
