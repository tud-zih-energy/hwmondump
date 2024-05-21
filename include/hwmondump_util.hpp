#include <fcntl.h>
#include <sensors/sensors.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <argparse/argparse.hpp>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <vector>

#include <metadata.hpp>
#include <libsensors_output_list.hpp>

using time_reading_storage = std::vector<std::pair<uint64_t, double>>;

static const std::string fname_suffix_timestamp_value = "_timestamp_value.csv";
static const std::string fname_suffix_duration_value = "_duration_value.csv";

/**
 * output content of a time_reading_storage object to an output file
 * -> uses csv format
 */
void outputstorage(time_reading_storage storage, std::filesystem::path path) {
  std::ofstream outputfile;
  outputfile.open(path.string());

  if (!outputfile.is_open()) {
    throw std::runtime_error("output file not open");
  }

  outputfile << "nanoseconds,value\n";

  for (const auto& sample : storage) {
    outputfile << std::fixed << sample.first << "," << sample.second << "\n";
  }

  outputfile.close();
}

/**
 * checks if an output file for a certain method already exists at the
 * destination
 * @throws std::runtime_error if file exists
 */
static void checkoutputfile(std::filesystem::path file) {
  if (std::filesystem::exists(file)) {
    throw std::runtime_error("cannot save: file already exists");
  }
}

/**
 * Checks if the output files for the given method in the given dir already
 * exist. Throws if files exist, otherwise silently passes.
 * @param method method to determine full path (and to add to logging)
 * @param o_path directory to output to
 * @throws std::runtime_error if one of the files exists
 */
void checkalloutputfiles(const std::string& method,
                         const std::filesystem::path& o_path) {
  checkoutputfile(o_path / (method + fname_suffix_timestamp_value));
  checkoutputfile(o_path / (method + fname_suffix_duration_value));
}

/**
 * calls on checkoutputfile() and outputstorage()
 * Note: overwrites if files already exist
 * @param o_path needs to end with "/"
 */
void save(time_reading_storage storage,
          time_reading_storage duration_value,
          std::string method,
          std::filesystem::path o_path) {
  checkalloutputfiles(method, o_path);

  outputstorage(storage, o_path / (method + fname_suffix_timestamp_value));
  outputstorage(duration_value,
                o_path / (method + fname_suffix_duration_value));
}

/**
 * @returns timestamp in nanoseconds
 */
uint64_t gettimestampnano() {
  // get epoch point
  auto since_epoch =
      std::chrono::high_resolution_clock::now().time_since_epoch();

  // return nanoseconds
  return std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch)
      .count();
}

/**
 * requires getvalue() function which returns a floating_point
 */
template <typename T>
concept Reader = requires(T t) {
  { t.getvalue() } -> std::floating_point;
  { T::methodname() } -> std::convertible_to<std::string>;
};

/**
 * starts 1 benchmark
 * calls gettimestampnano() and getvalue() accessnum times
 * @param storage will contain timestamp;value pairs after execution
 */
template <Reader R>
void benchmarkNum(const int accessnum,
                  const std::filesystem::path path,
                  time_reading_storage& storage) {
  R reader(path);

  for (int i = 0; i < accessnum; ++i) {
    // put data in vect
    storage[i] = {gettimestampnano(), reader.getvalue()};
  }
}

/**
 * starts one warmup run, lasting one second
 * does not save any meeasurements
 * @returns number of accesses in one second
 */
template <Reader R>
uint64_t benchmarkSec(const std::filesystem::path path) {
  R reader(path);
  uint64_t count = 0;

  // start timer
  std::chrono::time_point start = std::chrono::steady_clock::now();

  while ((std::chrono::steady_clock::now() - start) < std::chrono::seconds{1}) {
    // put data in vect
    gettimestampnano(), reader.getvalue();
    ++count;
  }

  return count;
}

/**
 * starts benchmark with 1/10 accesses of accessnum as warmup
 * then starts real benchmark with accessnum
 *
 * prints runtime estimate and actual runtime in ms
 */
template <Reader R>
void runbench(const int& accessnum,
              const std::filesystem::path path,
              time_reading_storage& storage) {
  // check if size is big enough
  if (storage.size() < accessnum) {
    throw std::out_of_range("storage too small");
  }

  int warmup_num = std::round(double(accessnum) / 10);

  // run benchmark warmup
  benchmarkNum<R>(warmup_num, path, storage);

  // dividing by 1000000 to get ms
  double Estimate =
    (double(storage[warmup_num - 1].first - storage[0].first) * 10) /
    1000000;
  std::cout << "        Time Estimate:     " << Estimate << " ms\n";

  // run real benchmark
  benchmarkNum<R>(accessnum, path, storage);
  double Runtime =
    double(storage[accessnum - 1].first - storage[0].first) / 1000000;
  std::cout << "        Real Runtime:      " << Runtime << " ms\n\n";
}

/**
 * creates value_duration vector
 * @returns vector of type time_reading_storage with the duration each value was
 * present
 */
time_reading_storage getvalueduration(const time_reading_storage& storage) {
  if (storage.empty()) {
    return {};
  }

  time_reading_storage dur_val_reversed;
  time_reading_storage::value_type::second_type current_value =
      storage.back().second;
  time_reading_storage::value_type::first_type current_value_timestamp =
      storage.back().first;

  // due to some weird quirk in our requirements, the scanning has to happen
  // backwards scanning forward or backward yield equally "valid" result (as
  // in "are equally valid interpretations"). However, they yield
  // **numerically different** results. So we scan backwards here.
  for (auto time_value_pair_it = storage.rbegin();
       time_value_pair_it != storage.rend(); ++time_value_pair_it) {
    // only act on new value
    if (time_value_pair_it->second != current_value) {
      dur_val_reversed.push_back(
          {current_value_timestamp - time_value_pair_it->first, current_value});
      current_value_timestamp = time_value_pair_it->first;
      current_value = time_value_pair_it->second;
    }
  }

  std::reverse(dur_val_reversed.begin(), dur_val_reversed.end());
  return dur_val_reversed;
}

/**
 * Reader class with method:
 * open - read - close - open - read - close - ...
 */
class ReaderSysfs {
 private:
  const std::filesystem::path path_;

 public:
  ReaderSysfs(const std::filesystem::path path) : path_(path) {}

  /**
   * returns string of method name
   */
  static const char* methodname() { return "sysfs"; };

  /**
   * opens a file, accesses it once and closes it
   * @returns content of said file as floating-point number
   * @throws std::runtime_error if open() didn't work
   * @throws std::runtime error if file is empty
   */
  double getvalue() {
    char filecontent[1024];

    // opens file with path from command line arg
    int fd = open(path_.c_str(), O_RDONLY);
    if (fd < 0) {
      throw std::runtime_error(
          "[sysfs] error with sensorfile handling, does your file exist?");
    }

    size_t bytesRead = read(fd, filecontent, sizeof(filecontent) - 1);

    if (bytesRead == 0) {
      close(fd);
      throw std::runtime_error("[sysfs] could not read sensor: file empty");
    }

    close(fd);

    // add null byte
    if (bytesRead >= 0) {
      filecontent[bytesRead] = 0;
    }

    return atof(filecontent);
  }

  ~ReaderSysfs() {}
};

/**
 * Reader class with method:
 * open - read - seek - read - seek - ... - close
 *
 * opens file in constructor, closes it in destructor
 */
class ReaderLseek {
 private:
  const std::string path_;
  int fd_;

 public:
  ReaderLseek(const std::string path)
      : path_(path), fd_(open(path_.c_str(), O_RDONLY)) {}

  /**
   * returns string of method name
   */
  static const char* methodname() { return "lseek"; };

  /**
   * reads content of previously opened file, sets cursor to beginning of file
   *
   * @returns current content of said file as floating-point number
   * @throws std::runtime_error if open() didn't work
   * @throws std::runtime error if file is empty
   */
  double getvalue() {
    char filecontent[1024];

    // checks if file is open
    if (fd_ < 0) {
      throw std::runtime_error(
          "[lseek] error with sensorfile handling, does your file exist?");
    }

    size_t bytesRead = read(fd_, filecontent, sizeof(filecontent) - 1);

    if (bytesRead == 0) {
      throw std::runtime_error("could not read sensor: file empty");
    }

    // add null byte
    if (bytesRead >= 0) {
      filecontent[bytesRead] = 0;
    }

    // reset position to beginning of file
    lseek(fd_, 0, SEEK_SET);

    return atof(filecontent);
  }

  // close file at end of programm
  ~ReaderLseek() { close(fd_); }
};

/**
 * Reader class with method:
 * call on libsensors plugin
 *
 * calls sensors_init() in constructor, sensors_cleanup() in destructor
 */
class ReaderLibsens {
 private:
  std::string path_;
  std::pair<const sensors_chip_name*, int> item_;

 public:
  // when reader is created it fills items_ list with sensor at path_
  ReaderLibsens(std::string path) : path_(path) {
    sensors_init(nullptr);
    SensorList sensor_list;

    for (const auto& sensor : sensor_list.sensors) {
      std::string sensor_path = sensor.chip_path + "/" + sensor.subfeature_name;
      if (sensor_path == path) {
        item_ = std::make_pair(sensor.chip_name, sensor.subfeature_num);
        return;
      }
    }

    throw std::runtime_error("could not find sensors (total " + std::to_string(sensor_list.sensors.size()) + " sensors availabel)");
  }

  /**
   * returns string of method name
   */
  static const char* methodname() { return "libsensors"; };

  /**
   * calls on libsensors plugin once to get value of certain sensor
   *
   * @returns current content of said file as double
   * @throws std::runtime_error if libsensors returns error code
   */
  double getvalue() {
    double value = 0;

    // first is chip name, second is subfeature number
    int is_error = sensors_get_value(item_.first, item_.second, &value);

    if (is_error != 0) {
      throw std::runtime_error("Error with libsensors call");
    }

    // parse to double
    double parsed_content = (double)value;

    return parsed_content;
  }

  ~ReaderLibsens() { sensors_cleanup(); };
};

/**
 * Reader class that returns 0
 * used for testing, no actual reading happens here
 */
class ReaderNull {
 public:
  ReaderNull(std::string path){};

  /**
   * returns string of method name
   */
  static const char* methodname() { return "null"; };

  /**
   * @returns 0
   */
  double getvalue() { return 0; }
};

/**
 * Runs the runbench() function with user-facing output
 * if accessnum is >0, perform time-based (auto-) determination of accessnum
 */
template <Reader R>
static void runbenchWrapper(int accessnum,
                            const int accesstime,
                            const std::filesystem::path& path,
                            const std::filesystem::path& output_path) {
  // check if outputfile(s) already exists
  checkalloutputfiles(R::methodname(), output_path);

  // determine update time
  if (accesstime > 0) {
    std::cout << "[" << R::methodname() << "] estimating number of accesses for " << accesstime << " s runtime...\n";
    auto accesses_per_second = benchmarkSec<R>(path);
    accessnum = accesses_per_second * accesstime;

    std::cout << "[" << R::methodname() << "] will perform " << accessnum << " accesses\n";
  }

  // create data storage
  time_reading_storage storage(accessnum);

  std::cout << "[" << R::methodname() << "] starting benchmark...\n";
  runbench<R>(accessnum, path, storage);

  std::cout << "[" << R::methodname() << "] postprocessing...\n";
  time_reading_storage duration_value = getvalueduration(storage);

  std::cout << "[" << R::methodname() << "] saving...\n";
  save(storage, duration_value, R::methodname(), output_path);

  std::cout << "[" << R::methodname() << "] done\n\n";
}

/**
 * fetches arguments for hwmondump record command and start corresponding
 * benchmarks
 * @throws std::runtime_error if no access method was given
 * @returns 0 on success
 * @returns -1 on failure
 */
int recordSubcommand(argparse::ArgumentParser& record_command) {
  int accessnum = 0;
  int accesstime = 0;
  std::filesystem::path output_path;
  std::filesystem::path path;

  if (record_command.is_used("--accesstime") &&
      record_command.is_used("--accessnum")) {
    std::cerr << "specify either --accessnum or --accesstime\n";
    return -1;
  }

  // should be equal / greater than 10
  if (record_command.is_used("--accesstime")) {
    accesstime = record_command.get<int>("accesstime");

    if (accesstime <= 0) {
      std::cerr << "accesstime too short, please enter at least 1s\n";
      return -1;
    }
  }

  if (record_command.is_used("--accessnum")) {
    accessnum = record_command.get<int>("accessnum");
    if (accessnum < 10) {
      std::cerr << "accessnumber too small, see --help\n";
      return -1;
    }
  }

  if (0 == accessnum && 0 == accesstime) {
    // load default argument
    accesstime = record_command.get<int>("accesstime");
  }

  output_path = record_command.get<std::string>("--output");
  path = record_command.get<std::string>("SENSOR");

  // if output directory doesnt exist: create it
  if (!std::filesystem::exists(output_path)) {
    bool created = std::filesystem::create_directories(output_path);

    if (!created) {
      throw std::runtime_error("could not create output directory");
    }
  }

  std::cout << "Path: " << path << "\n";

  // prepare metadata beforehand, but dump *after* experiments
  const auto metadata_path =
      output_path / std::filesystem::path("metadata.toml");
  Metadata metadata;
  if (!record_command.is_used("--no-metadata")) {
    if (std::filesystem::exists(metadata_path)) {
      std::cerr << "metadata file already exists at " << metadata_path << "\n";
      return -1;
    }
    // genearte metadata
    metadata = {
        .sensor_path = path,
    };
    if (0 != accessnum) {
      metadata.accessnum = accessnum;
    }
    if (0 != accesstime) {
      metadata.accesstime_s = accesstime;
    }

    metadata.autofill();
  }

  try {
    // check what methods were used
    if (record_command.is_used("--sysfs")) {
      runbenchWrapper<ReaderSysfs>(accessnum, accesstime, path, output_path);
    }
    if (record_command.is_used("--sysfs-lseek")) {
      runbenchWrapper<ReaderLseek>(accessnum, accesstime, path, output_path);
    }
    if (record_command.is_used("--libsensors")) {
      runbenchWrapper<ReaderLibsens>(accessnum, accesstime, path, output_path);
    }
    if (record_command.is_used("--null")) {
      runbenchWrapper<ReaderNull>(accessnum, accesstime, path, output_path);
    }
    if (!(record_command.is_used("--sysfs") ||
          record_command.is_used("--sysfs-lseek") ||
          record_command.is_used("--libsensors") ||
          record_command.is_used("--null"))) {
      std::cerr << "Select at least one readout method from --sysfs, "
                   "--sysfs-lseek, --libsensors, or --null (see --help)\n";
      return -1;
    }

  } catch (const std::runtime_error& e) {
    std::cerr << e.what() << "\n";
    return -1;
  }

  if (!record_command.is_used("--no-metadata")) {
    // only store metadata on success
    metadata.save(metadata_path);
  }

  return 0;
}
