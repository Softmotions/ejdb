require "rbejdb"
require 'test/unit'

TESTDB_DIR = 'testdb'

unless File.exists?(TESTDB_DIR)
  Dir.mkdir TESTDB_DIR
end

Dir.chdir TESTDB_DIR

$jb = EJDB.open("tdbt3", EJDB::JBOWRITER | EJDB::JBOCREAT | EJDB::JBOTRUNC)

class EJDBAdvancedTestUnit < Test::Unit::TestCase
  RS = 100000
  QRS = 100

  def test_ejdbadv1_performance
    assert_not_nil $jb
    assert $jb.open?

    puts "Generating test batch"

    recs = []
    letters = ('a'..'z').to_a
    (1..RS).each { |i|
      recs.push({
                    :ts => Time.now,
                    :rstring => (0...rand(128)).map { letters.sample }.join
                }
      )
      if i % 10000 == 0
        puts "#{i} records generated"
      end

    }

    puts "Saving..."
    st = Time.now
    recs.each { |rec| $jb.save("pcoll1", rec) }
    puts "Saved #{RS} objects, time: #{Time.now - st} s"

    puts "Checking saved..."
    assert_equal(RS, $jb.find("pcoll1", {}, :onlycount => true))
    recs.each { |rec|
      assert_equal(1, $jb.find("pcoll1", rec, :onlycount => true))
    }

    $jb.drop_string_index("pcoll1", "rstring")

    puts "Quering..."

    st = Time.now
    (1..QRS).each {
      $jb.find("pcoll1", recs.sample, :onlycount => true)
    }
    secs = Time.now - st
    puts "#{QRS} queries, time: #{secs} s, #{'%.7f' % (secs / QRS)} s per query"


    puts "Setting index..."

    st = Time.now
    $jb.ensure_string_index("pcoll1", "rstring")
    puts "Index built in #{Time.now - st} s"

    puts "Quering again..."

    st = Time.now
    (1..QRS).each {
      $jb.find("pcoll1", recs.sample, :onlycount => true)
    }
    secs = Time.now - st
    puts "#{QRS} queries with rstring index, time: #{secs} s, #{'%.7f' % (secs / QRS)} s per query"


    puts "Quering with $set..."
    st = Time.now
    assert_equal(RS, $jb.update("pcoll1", {"$set" => {"intv" => 1}}))
    puts "Update query ($set) time: #{Time.now - st}"

    puts "Quering with $inc..."
    st = Time.now
    assert_equal(RS, $jb.update("pcoll1", {"$inc" => {"intv" => 1}}))
    puts "Update query ($inc) time: #{Time.now - st}"

    assert_equal(RS, $jb.find("pcoll1", {"intv" => 2}, :onlycount => true))

  end
end
