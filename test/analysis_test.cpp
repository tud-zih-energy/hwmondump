#include <config.h>
#include <algorithm>
#include <analysis_util.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <fstream>

void WriteMockCSV(std::vector<uint64_t> timestamps) {
  std::ofstream mockCSV(TEST_BINARY_DIR "/test_timestamp_value.csv");

  mockCSV << "nanoseconds,value\n";
  for (const auto& value : timestamps) {
    mockCSV << value << ",42\n";
  }

  mockCSV.close();
}

// doesn't include check if timestamps are read correctly, since they won't be
// saved and only the calculated durations are important to move on
TEST_CASE("get durations from file") {
  SECTION("minimal") {
    WriteMockCSV({2, 3});

    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");
    std::vector<uint64_t> durations;
    a_file.getDurations(durations);

    REQUIRE(durations.size() == 1);
    REQUIRE(durations[0] == 1);
  }

  SECTION("full") {
    WriteMockCSV({2, 6, 12, 34, 54});

    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");
    std::vector<uint64_t> durations;
    a_file.getDurations(durations);

    REQUIRE(durations.size() == 4);
    // gets ordered by nth_element
    REQUIRE(durations[0] == 4);
    REQUIRE(durations[1] == 6);
    REQUIRE(durations[2] == 20);
    REQUIRE(durations[3] == 22);
  }

  SECTION("timestamps not sorted") {
    WriteMockCSV({4, 7, 2, 10, 3});

    REQUIRE_THROWS_WITH(
        ReadingFile(TEST_BINARY_DIR "/test_timestamp_value.csv"),
        "detected unordered timestamps in file, was it created by hwmondump "
        "record?");
  }
}

TEST_CASE("Median of file") {
  SECTION("no values") {
    REQUIRE_THROWS_WITH(
        ReadingFile(TEST_SOURCE_DIR "/test_timestamp_value.csv"),
        "Not enough timestamps available in file, can't calculate median");
  }

  SECTION("one value") {
    WriteMockCSV({2});

    REQUIRE_THROWS_WITH(
        ReadingFile(TEST_BINARY_DIR "/test_timestamp_value.csv"),
        "Not enough timestamps available in file, can't calculate median");
  }

  SECTION("minimal even numbers") {
    WriteMockCSV({2, 10});  // duration = 8

    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");

    REQUIRE(a_file.getMedian() == 8);
  }

  SECTION("minimal uneven numbers") {
    WriteMockCSV({2, 10, 14});  // duration = 8,4

    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");

    REQUIRE(a_file.getMedian() == 6);
  }

  SECTION("even number of timestamps") {
    WriteMockCSV({6, 7, 10, 19});  // durations = 1,3,9

    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");

    REQUIRE(a_file.getMedian() == 3);
  }

  SECTION("uneven number of timestamps") {
    WriteMockCSV({4, 7, 9, 13, 14});  // durations = 3,2,4,1
    ReadingFile a_file(TEST_BINARY_DIR "/test_timestamp_value.csv");

    REQUIRE(a_file.getMedian() == 2.5);
  }
}

TEST_CASE("find files") {
  SECTION("no output files found") {
    REQUIRE_THROWS_WITH(
        ReadingsDirectory(TEST_SOURCE_DIR "/test_dir_empty/"),
        "No files to analyze, directory doesn't contain output files");
  }

  SECTION("functional example") {
    ReadingsDirectory dir(TEST_SOURCE_DIR "/test_dir/");

    std::vector<ReadingFile> f;
    dir.getFiles(f);

    REQUIRE(f.size() == 1);
    REQUIRE(f[0].getPath() == TEST_SOURCE_DIR
            "/test_dir/dontignore_timestamp_value.csv");
  }
}

TEST_CASE("read metadata") {
  SECTION("no metadata") {
    ReadingsDirectory rd(TEST_SOURCE_DIR "/test_dir_no_metadata/");
    REQUIRE("" == rd.sensor_path());
    REQUIRE("" == rd.uuid());
  }

  SECTION("with metadata") {
    ReadingsDirectory rd(TEST_SOURCE_DIR "/test_dir/");
    REQUIRE("my_sensorpath" == rd.sensor_path());
    REQUIRE("174813aa-d6a2-4bf4-8ac6-c55b13c97d32" == rd.uuid());
  }

  SECTION("incomplete metadata") {
    REQUIRE_THROWS_WITH(ReadingsDirectory(TEST_SOURCE_DIR "/test_dir_malformed_metadata/"),
                        "metadata.toml must contain information on sensor path");
  }
}

TEST_CASE("get median from dir") {
  ReadingsDirectory dir(TEST_SOURCE_DIR "/test_dir/");

  std::vector<ReadingFile> files;
  dir.getFiles(files);

  REQUIRE(files.size() == 1);

  REQUIRE(dir.getMedian(files[0]) == 10);
}

TEST_CASE("simple csv output") {
  REQUIRE("sensor_path,uuid,sysfs_ns,sysfs_lseek_ns,libsensors_ns,null_ns" == ReadingsDirectory::csv_header());

  ReadingsDirectory rd(TEST_SOURCE_DIR "/full_run/");
  REQUIRE(rd.csv() == "/sys/class/hwmon/hwmon5/temp1_input,8eb5bfce-ed49-4542-b1e6-0e60fe172ce4,5019.000000,NA,6202.000000,NA");
}
