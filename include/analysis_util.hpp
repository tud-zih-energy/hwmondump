#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <toml++/toml.hpp>

/**
 * Class that represents one file of a directory that contains
 * time-value-pairs in CSV format, as produced by outputstorage()
 *
 * Calculates median in constructor
 * Use getMedian() to access values
 * Use getPath() to see which file was analyzed
 *
 * durations will always be sorted in the constructor, so using getDurations()
 * won't return the actual sequence of durations
 */
class ReadingFile {
 private:
  std::filesystem::path path_;
  std::vector<uint64_t> durations_;
  double median;

  /**
   * reads timestamps of a timestamp_value.csv file, then calculates durations
   * between the timestamps
   *
   * durations can be accessed through Readingfile.getDurations()
   */
  void fillDurations() {
    std::vector<uint64_t> timestamps;
    bool first = true;

    std::ifstream curr_file(path_);

    if (!curr_file.is_open()) {
      std::cerr << "check path: " << path_.string() << "\n";
      throw std::runtime_error("csv file not open");
    }

    // read first column (=timestamps) of csv into vector
    for (std::string line; getline(curr_file, line);) {
      // split string at ","
      std::string time = line.substr(0, line.find(","));

      // skip first line
      if (first) {
        first = false;
        continue;

      } else {
        timestamps.push_back(std::stoll(time));
      }
    }

    curr_file.close();

    // check if median can be calculated
    if (timestamps.size() <= 1) {
      throw std::runtime_error(
          "Not enough timestamps available in file, can't calculate median");
    }

    // check if timestamps are in ascending order
    if (!std::is_sorted(timestamps.begin(), timestamps.end())) {
      throw std::runtime_error(
          // ignore the weird formatting of this error message pls
          "detected unordered timestamps in file, was it created by hwmondump "
          "record?");
    }

    for (int i = 0; i < timestamps.size() - 1; ++i) {
      // next timestamp - current timestamp = duration
      durations_.push_back(timestamps[i + 1] - timestamps[i]);
    }
  }

 public:
  ReadingFile(const std::filesystem::path& path) : path_(path) {
    fillDurations();

    // check size
    if (durations_.size() == 1) {
      median = durations_[0];

    } else {
      if (durations_.size() % 2 == 0) {
        size_t pos1 = (durations_.size() / 2);
        size_t pos2 = (durations_.size() / 2) - 1;

        // nth_element is a partial sorting algorithm that rearranges elements
        // in [first, last) such that the element pointed at by nth is changed
        // to whatever element would occur in that position if [first, last)
        // were sorted.
        nth_element(durations_.begin(), durations_.begin() + pos1,
                    durations_.end());
        nth_element(durations_.begin(), durations_.begin() + pos2,
                    durations_.end());

        median = (double(durations_[pos1]) + double(durations_[pos2])) / 2;

      } else {
        size_t pos = durations_.size() / 2;

        nth_element(durations_.begin(), durations_.begin() + pos,
                    durations_.end());

        median = double(durations_[pos]);
      }
    }
  }

  /**
   * @param vec Vector that will be filled with durations calcluated from
   * timestamps in a timestmap_value.csv file
   * */
  void getDurations(std::vector<uint64_t>& vec) const { vec = durations_; }

  /**
   * @returns path to this ReadingFile object
   */
  std::string getPath() const { return path_.string(); }

  /**
   * @returns median of durations from this Readingfile
   */
  double getMedian() const { return median; }

  /**
   * @returns string of access method that was used for this ReadingFile
   */
  std::string getMethod() const {
    std::string filename = path_.filename();
    return filename.substr(0, filename.find("_"));
  }
};

/**
 * Class that contains all sensor output files of one directory
 *
 * Use getFiles(std::vector<ReadingFiles>) to get a list of timestamp_value.csv
 * files in this directory
 */
class ReadingsDirectory {
 private:
  std::filesystem::path path_;
  std::vector<ReadingFile> files_;
  std::string sensor_path_;
  std::string uuid_;

 public:
  ReadingsDirectory(const std::filesystem::path& path) : path_(path) {
    // fill files vector
    for (const auto& entry : std::filesystem::directory_iterator(path_)) {
      std::filesystem::path path = entry.path();

      // check which files to add
      if (path.string().ends_with("_timestamp_value.csv")) {
        files_.emplace_back(path);
      }
    }

    if (files_.size() == 0) {
      throw std::runtime_error(
          "No files to analyze, directory doesn't contain output files");
    }

    // determine read sensorpath
    auto metadata_path = path / "metadata.toml";
    if (std::filesystem::is_regular_file(metadata_path)) {
      toml::table tbl;
      try {
        tbl = toml::parse_file(metadata_path.native());
      } catch (const toml::parse_error& err) {
        throw std::runtime_error("metadata parsing failed: " + std::string(err.description()));
      }

      if (!tbl["sensor_path"].is_string()) {
        throw std::runtime_error("metadata.toml must contain information on sensor path");
      }
      if (!tbl["uuid"].is_string()) {
        throw std::runtime_error("metadata.toml must contain uuid");
      }

      sensor_path_ = tbl["sensor_path"].value_or<std::string>("");
      uuid_ = tbl["uuid"].value_or<std::string>("");
    }
  }

  /**
   * path of sensor read in this directory
   */
  std::string sensor_path() const {
    return sensor_path_;
  }

  /**
   * UUID of experiment in this directory
   */
  std::string uuid() const {
    return uuid_;
  }

  /**
   * @returns path to direrctory which will be analyzed
   */
  std::string getDir() const { return path_; }

  /**
   * @returns vector of all timestamp_value.csv files in the directory,
   * contains ReadingFile objects
   */
  void getFiles(std::vector<ReadingFile>& vec) const { vec = files_; }

  /**
   * @param FilePath can be equal to ReadingFile.getPath()
   * @returns median of one file
   */
  double getMedian(const ReadingFile& readingfile) const {
    for (const auto& file : files_) {
      if (readingfile.getPath() == file.getPath()) {
        return file.getMedian();
      }
    }

    throw std::runtime_error("no file with this name");
  }

  static std::string csv_header() {
    return "sensor_path,uuid,sysfs_ns,sysfs_lseek_ns,libsensors_ns,null_ns";
  }

  std::string csv() const {
    std::string libsensors_ns = "NA";
    std::string sysfs_ns = "NA";
    std::string sysfs_lseek_ns = "NA";
    std::string null_ns = "NA";

    for (const auto& f : files_) {
      // fill csv for available methods
      if ("sysfs" == f.getMethod()) {
        sysfs_ns = std::to_string(getMedian(f));
      } else if ("lseek" == f.getMethod()) {
        sysfs_lseek_ns = std::to_string(getMedian(f));
      } else if ("libsensors" == f.getMethod()) {
        libsensors_ns = std::to_string(getMedian(f));
      } else if ("null" == f.getMethod()) {
        null_ns = std::to_string(getMedian(f));
      }
    }

    return sensor_path() + "," + uuid() + "," + sysfs_ns + "," + sysfs_lseek_ns + "," + libsensors_ns + "," + null_ns;
  }
};

/**
 * Parses needed arguments and calls on getMedian()
 * handles output
 *
 * @returns 0 on success
 */
int startAnalysis(const std::string& dir, bool as_csv) {
  // create ReadingsDirecty object and get all timestamp_value.csv files
  ReadingsDirectory results(dir);
  std::vector<ReadingFile> files;
  results.getFiles(files);

  if (as_csv) {
    std::cout << results.csv() << std::endl;
  } else {
    // human-readable output
    for (const auto& file : files) {
      std::cout << std::fixed << file.getMethod() << ": "
                << uint64_t(results.getMedian(file)) << " nanoseconds\n";
    }
  }

  return 0;
}
