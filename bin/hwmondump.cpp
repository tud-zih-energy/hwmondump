#include <analysis_util.hpp>
#include <argparse/argparse.hpp>
#include <libsensors_output_list.hpp>
#include <hwmondump_util.hpp>

int main(int argc, char* argv[]) {
  argparse::ArgumentParser program("hwmondump");

  argparse::ArgumentParser list_command("list");
  list_command.add_description("list all available sensors");

  argparse::ArgumentParser about_command("about");
  about_command.add_description("print information about hwmondump");

  argparse::ArgumentParser analysis_command("analysis");
  analysis_command.add_description(
      "analyze benchmark data produced by hwmondump record");
  analysis_command.add_argument("--median")
      .help("caluclate median of all sensor recordings in a directory")
      .flag();
  analysis_command.add_argument("-d", "--directory")
      .help(
          "directory of benchmark files you want to analyze, must contain "
          "files created by hwmondump record")
      .default_value("./")
      .metavar("DIR");
  analysis_command.add_argument("--csv")
      .help("print output as CSV in pre-defined format")
      .flag();
  analysis_command.add_argument("--csv-header")
      .help("print header for --csv and exit")
      .flag();

  argparse::ArgumentParser record_command("record");
  record_command.add_description("access a sensor");

  // required parameters
  record_command.add_argument("SENSOR").help(
      "path of sensor to read, e.g.: /sys/class/hwmon/hwmon5/temp1_input");

  record_command.add_argument("--sysfs").help("use sysfs method").flag();
  record_command.add_argument("--sysfs-lseek").help("use lseek method").flag();
  record_command.add_argument("--libsensors")
      .help("use libsensors method")
      .flag();

  record_command.add_argument("--null")
      .help("tests the speed of this program without accessing sensors")
      .flag();

  // optional parameters
  record_command.add_argument("-a", "--accessnum")
      .help("how often the sensor will be accessed, must be at least 10")
      .scan<'d', int>()
      .metavar("NUM");

  record_command.add_argument("-t", "--accesstime")
      .help("record a sensor for t seconds instead of using an access number")
      .scan<'d', int>()
      .metavar("SEC")
      .required()
      .default_value(5);

  record_command.add_argument("-o", "--output")
      .help("directory for saving output files")
      .metavar("DIR")
      .required()
      .default_value("./");

  record_command.add_argument("--no-metadata")
      .help("do not store metadata in metadata.toml")
      .flag();

  program.add_subparser(list_command);
  program.add_subparser(record_command);
  program.add_subparser(analysis_command);
  program.add_subparser(about_command);

  // true if no arguments were given
  if (argc <= 1) {
    std::cerr << program;
    return -1;
  }

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return -1;
  }

  if (program.is_subcommand_used("list")) {
    return printSensorList();

  } else if (program.is_subcommand_used("record")) {
    return recordSubcommand(record_command);

  } else if (program.is_subcommand_used("analysis")) {
    if (analysis_command.is_used("--csv-header")) {
      std::cout << ReadingsDirectory::csv_header() << std::endl;
      return 0;
    }

    std::string dir = analysis_command.get<std::string>("-d");

    bool as_csv = analysis_command.is_used("--csv");

    if (analysis_command.is_used("--median")) {
      return startAnalysis(dir, as_csv);
    } else {
      throw std::runtime_error("missing analysis goal, see --help");
    }
  } else if (program.is_subcommand_used("about")) {
    std::cout << R"abouttext(hwmondump - read hwmon sensor data
Copyright (C) Technische Universität Dresden
Built by Tessa Todorowski and Hannes Tröpgen

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
)abouttext";
    return 0;
  }
  return 0;
}
