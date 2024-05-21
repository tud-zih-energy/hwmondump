#pragma once

#include <string>
#include <system_error>
#include <cerrno>
#include <fstream>
#include <cstdint>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <optional>

extern "C" {
#include <unistd.h>
#include <uuid/uuid.h>
}

#include <toml++/toml.hpp>

#include <libcpuid/libcpuid.h>

static cpu_id_t get_cpu_info() {
  // contains only raw data
  struct cpu_raw_data_t raw;
  // contains recognized CPU features data
  struct cpu_id_t data;

  // obtain the raw CPUID data
  if (cpuid_get_raw_data(&raw) < 0) {
    throw std::runtime_error(std::string("can not retrieve CPUID data: ") + cpuid_error());
  }

  // identify the CPU, using the given raw data.
  if (cpu_identify(&raw, &data) < 0) {                                   
    throw std::runtime_error(std::string("could not identify CPU: ") + cpuid_error());
  }

  return data;
}

static std::string generate_uuid() {
  // from util-linux lib -- compatible, but requires usage of C-style API >_<
  uuid_t uuid;
  char uuid_str[37]; // 36 bytes + NUL

  uuid_generate_random(uuid);
  uuid_unparse_lower(uuid, uuid_str);
  return std::string(uuid_str);
}

/**
 * Contains Metadata associated to one measurement.
 * The intended workflow is as followed:
 * 
 * 1. Configure experiment
 * 2. fill Metadata object
 * 3. Execute experiment
 * 4. Write Metadata object to disk
 *
 * There is a debate to be had, if metadata should be stored before or after the run.
 * However, as writing a metadata object and then failing for a wrong file path would really be annoying, here, the choice is to store after the experiment.
 */
class Metadata {
public:
  /// path of read sensor in sysfs
  std::string sensor_path;

  /// number of sensor accesses (if given, otherwise null)
  std::optional<int> accessnum;

  /// (desired) time limit in second
  std::optional<int> accesstime_s;

  /// start of experiment
  std::chrono::system_clock::time_point start_datetime;

  /// (random) UUID of experiment
  std::string uuid;

  /// hostname for easy identification of experiments
  std::string hostname;

  /// cpu family (as reported by /proc/cpu)
  int32_t cpu_family;
  /// cpu model (as reported by /proc/cpu)
  int32_t cpu_model;

  /// codename of CPU (shorter, more useful name -- non-canonical, given by library)
  std::string cpu_codename;

  /// string describing the CPU vendor
  std::string cpu_vendor_name;

  /// full CPU name as string
  std::string cpu_brand_name;

  /// attempt to fill most attributes automatically
  void autofill() {
    // set time
    start_datetime = std::chrono::system_clock::now();

    uuid = generate_uuid();

    // load hostname
    char buf[512];
    int rc = gethostname(buf, sizeof(buf));
    if (0 != rc) {
      throw std::system_error(errno, std::generic_category(), "could not retrieve hostname");
    }
    hostname = std::string(buf);

    // load cpu information
    auto cpu_info = get_cpu_info();
    cpu_family = cpu_info.ext_family;
    cpu_model = cpu_info.ext_model;
    cpu_codename = cpu_info.cpu_codename;
    cpu_vendor_name = cpu_info.vendor_str;
    cpu_brand_name = cpu_info.brand_str;
  };

  std::string start_datetime_str() const {
    std::time_t t_struct = std::chrono::system_clock::to_time_t(start_datetime);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t_struct), "%F %T");
    return ss.str();
  }

  void save(const std::filesystem::path& fname) {
    if (accessnum && accesstime_s) {
      throw std::runtime_error("can only set one of accessnum and accesstime");
    }
    
    std::ofstream f(fname);
    if (!f.is_open()) {
      throw std::runtime_error("could not open metadata file for writing: " + fname.native());
    }

    auto doc_root = toml::table{
      {"hostname", hostname},
      {"sensor_path", sensor_path},
      {"start_datetime", start_datetime_str()},
      {"uuid", uuid},
      {"cpu", toml::table{
          {"family", cpu_family},
          {"model", cpu_model},
          {"vendor", cpu_vendor_name},
          {"brand_name", cpu_brand_name},
          {"codename", cpu_codename},
        }}
    };

    if (accessnum) {
      doc_root.emplace("accessnum", *accessnum);
    }

    if (accesstime_s) {
      doc_root.emplace("accesstime_s", *accesstime_s);
    }

    f << doc_root;
  }
};
