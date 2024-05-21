#include <config.h>
#include <unistd.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <hwmondump_util.hpp>
#include <type_traits>
#include <metadata.hpp>
#include <ctime>

TEST_CASE("reading takes over 1 second") {
  uint64_t time_start = gettimestampnano();

  REQUIRE(usleep(500000) == 0);

  uint64_t time_middle = gettimestampnano();

  REQUIRE(usleep(500000) == 0);

  uint64_t time_end = gettimestampnano();

  REQUIRE((time_start < time_middle && time_middle < time_end));
}

TEST_CASE("Sysfs class start to finish") {
  SECTION("working") {
    ReaderSysfs reader(TEST_SOURCE_DIR "/test_file.txt");

    double filecontent = reader.getvalue();

    double expected = 42;

    REQUIRE(filecontent == expected);
  }

  SECTION("file errors") {
    ReaderSysfs reader(TEST_SOURCE_DIR "/test_file2.txt");
    REQUIRE_THROWS(reader.getvalue());

    ReaderSysfs reader2("./wrong_path.txt");
    REQUIRE_THROWS(reader2.getvalue());
  }
}

TEST_CASE("Lseek class start to finish") {
  SECTION("working") {
    ReaderLseek reader(TEST_SOURCE_DIR "/test_file.txt");

    double filecontent = reader.getvalue();

    double expected = 42;

    REQUIRE(filecontent == expected);
  }

  SECTION("file errors") {
    ReaderLseek reader(TEST_SOURCE_DIR "/test_file2.txt");

    REQUIRE_THROWS(reader.getvalue());

    ReaderLseek reader2("./wrong_path.txt");

    REQUIRE_THROWS(reader2.getvalue());
  }
}

TEST_CASE("benchmarkNum func") {
  time_reading_storage storageHw;
  storageHw.resize(1);
  time_reading_storage storageLs;
  storageLs.resize(1);

  benchmarkNum<ReaderSysfs>(1, TEST_SOURCE_DIR "/test_file.txt", storageHw);
  benchmarkNum<ReaderLseek>(1, TEST_SOURCE_DIR "/test_file.txt", storageLs);

  REQUIRE(storageHw.size() == 1);
  REQUIRE(storageHw[0].second == 42);
  REQUIRE(storageLs.size() == 1);
  REQUIRE(storageLs[0].second == 42);
}

TEST_CASE("benchmarkSec func") {
  REQUIRE(benchmarkSec<ReaderSysfs>(TEST_SOURCE_DIR "/test_file.txt") != 0);
  REQUIRE(benchmarkSec<ReaderLseek>(TEST_SOURCE_DIR "/test_file.txt") != 0);
}

TEST_CASE("runbench func") {
  time_reading_storage storage;
  SECTION("storage too small accessnum") {
    storage.resize(2);
    REQUIRE_THROWS_WITH(
        runbench<ReaderSysfs>(10, TEST_SOURCE_DIR "/test_file.txt", storage),
        "storage too small");
  }

  // accesstime method has no "warmup" like accessnum, it's warmup is the
  // benchmarkSec function

  SECTION("working example warmup accessnum") {
    storage.clear();
    storage.resize(11);
    // to test if storage[10] was edited, since only 0-9 should be touched
    storage[10] = {6, 9};
    runbench<ReaderSysfs>(10, TEST_SOURCE_DIR "/test_file.txt", storage);

    REQUIRE(storage.size() == 11);
    REQUIRE(storage[10].first == 6);
    REQUIRE(storage[10].second == 9);
    REQUIRE(storage[9].second == 42);
  }

  SECTION("working example real accessnum") {
    storage.resize(10);
    storage[9] = {6, 9};
    runbench<ReaderSysfs>(10, TEST_SOURCE_DIR "/test_file.txt", storage);

    REQUIRE(storage.size() == 10);
    REQUIRE(storage[9].first != 6);
    REQUIRE(storage[9].second == 42);
  }
}

TEST_CASE("get value duration func") {
  SECTION("super minimal") {
    time_reading_storage storage = {{0, 0}, {2, 3}};
    time_reading_storage dur_val;

    dur_val = getvalueduration(storage);

    REQUIRE(dur_val.size() == 1);
    REQUIRE(dur_val[0].first == 2);
    REQUIRE(dur_val[0].second == 3);
  }

  SECTION("minimal with doubles") {
    time_reading_storage storage = {{0, 0}, {2, 3}, {7, 3}};
    time_reading_storage dur_val;

    dur_val = getvalueduration(storage);

    REQUIRE(dur_val.size() == 1);
    REQUIRE(dur_val[0].first == 7);
    REQUIRE(dur_val[0].second == 3);
  }

  SECTION("minimal no doubles") {
    time_reading_storage storage = {{0, 0}, {2, 20}, {5, 12}};
    time_reading_storage dur_val;

    dur_val = getvalueduration(storage);

    REQUIRE(dur_val.size() == 2);
    REQUIRE(dur_val[0].first == 2);
    REQUIRE(dur_val[0].second == 20);
    REQUIRE(dur_val[1].first == 3);
    REQUIRE(dur_val[1].second == 12);
  }

  SECTION("normal example") {
    time_reading_storage storage = {{0, 0},   {1, 17},  {3, 20},
                                    {4, 21},  {6, 21},  {7, 21},
                                    {10, 19}, {13, 19}, {15, 17}};
    time_reading_storage dur_val;

    dur_val = getvalueduration(storage);

    REQUIRE(dur_val.size() == 5);
    REQUIRE(dur_val[0].first == 1);
    REQUIRE(dur_val[0].second == 17);
    REQUIRE(dur_val[1].first == 2);
    REQUIRE(dur_val[1].second == 20);
    REQUIRE(dur_val[2].first == 4);
    REQUIRE(dur_val[2].second == 21);
    REQUIRE(dur_val[3].first == 6);
    REQUIRE(dur_val[3].second == 19);
    REQUIRE(dur_val[4].first == 2);
    REQUIRE(dur_val[4].second == 17);
  }
}

TEST_CASE("file output") {
  time_reading_storage storage;
  storage.resize(3);
  std::string filecontent;

  runbench<ReaderSysfs>(3, TEST_SOURCE_DIR "/test_file.txt", storage);

  // open file and read content
  std::ifstream outputfile;
  outputfile.open(TEST_SOURCE_DIR "../outputstorage.csv");
  filecontent = outputfile.get();
  outputfile.close();

  // check content
  REQUIRE(!filecontent.empty());
}

TEST_CASE("output file already exists") {
  time_reading_storage storage = {{1, 1}, {2, 2}};

  // do this so that pipeline works and it doesn't throw locally
  try {
    save(storage, storage, "test", TEST_BINARY_DIR);
  } catch (const std::exception&) {
  }

  REQUIRE_THROWS_WITH(save(storage, storage, "test", TEST_BINARY_DIR),
                      "cannot save: file already exists");
}

TEST_CASE("metadata") {
  SECTION("dump minimal") {
    Metadata m;
    std::string fname(TEST_BINARY_DIR "/metadata_test.toml");

    REQUIRE(!std::filesystem::exists(fname));
    m.save(fname);
    REQUIRE(std::filesystem::exists(fname));

    std::filesystem::remove(fname);
    REQUIRE(!std::filesystem::exists(fname));
  }

  SECTION("check autofill") {
    Metadata m;
    m.autofill();
    REQUIRE(!m.uuid.empty());
    REQUIRE(!m.hostname.empty());
    REQUIRE(0 != m.cpu_family);
    REQUIRE(0 != m.cpu_model);
    REQUIRE(!m.cpu_codename.empty());
    REQUIRE(!m.cpu_vendor_name.empty());
    REQUIRE(!m.cpu_brand_name.empty());
    REQUIRE(std::chrono::system_clock::time_point() != m.start_datetime);
  }

  SECTION("datetime conversion") {
    // creating a new datetime in C++ with known data is near-impossible, so just ensure that updates change the string
    Metadata m;
    const auto default_datetime = m.start_datetime_str();
    m.autofill();
    REQUIRE(!m.start_datetime_str().empty());
    REQUIRE(m.start_datetime_str() != default_datetime);
  }

  SECTION("full scenario") {
    Metadata m{
      // use only ascii strings, to ensure they are found in verbatim in the output
      .sensor_path = "asdhjasd",
      .accessnum = 12738123,
      .uuid = "6718236bnasd",
      .hostname = "akjsdhkjahkjlsa",
      .cpu_family = 17,
      .cpu_model = 42,
      .cpu_codename = "hjashdkljashdklj",
      .cpu_vendor_name = "has7d87123hn",
      .cpu_brand_name = "qwgheui123",
    };

    // file io stuff, may be ignored
    std::string fname(TEST_BINARY_DIR "/metadata_test.toml");
    REQUIRE(!std::filesystem::exists(fname));
    m.save(fname);
    REQUIRE(std::filesystem::exists(fname));

    std::stringstream ss;
    std::ifstream f(fname);

    // quickly clean up before other errors could occur
    std::filesystem::remove(fname);
    REQUIRE(!std::filesystem::exists(fname));

    REQUIRE(f.is_open());
    ss << f.rdbuf();
    std::string content = ss.str();

    // actual testing
    REQUIRE(!content.empty());
    REQUIRE(content.contains(m.sensor_path));
    REQUIRE(content.contains(std::to_string(*m.accessnum)));
    REQUIRE(content.contains(m.uuid));
    REQUIRE(content.contains(m.hostname));
    REQUIRE(content.contains(std::to_string(m.cpu_family)));
    REQUIRE(content.contains(std::to_string(m.cpu_model)));
    REQUIRE(content.contains(m.cpu_codename));
    REQUIRE(content.contains(m.cpu_vendor_name));
    REQUIRE(content.contains(m.cpu_brand_name));

    // just ensure key is present, do not mention formatting
    REQUIRE(content.contains("start_datetime"));
  }

  SECTION("no accesstime_s included by default") {
    Metadata m;
    std::string fname(TEST_BINARY_DIR "/metadata_test.toml");

    REQUIRE(!std::filesystem::exists(fname));
    m.save(fname);
    REQUIRE(std::filesystem::exists(fname));

    std::stringstream ss;
    std::ifstream f(fname);

    std::filesystem::remove(fname);
    REQUIRE(!std::filesystem::exists(fname));

    REQUIRE(f.is_open());
    ss << f.rdbuf();
    std::string content = ss.str();

    REQUIRE(!content.contains("accesstime_s"));
  }

  SECTION("accesstime and accesnum are mutually exclusive") {
    Metadata m;
    std::string fname(TEST_BINARY_DIR "/metadata_test.toml");
    m.accesstime_s = 1;
    m.accessnum = 1;

    REQUIRE_THROWS(m.save(fname));
    REQUIRE(!std::filesystem::exists(fname));
  }
}
