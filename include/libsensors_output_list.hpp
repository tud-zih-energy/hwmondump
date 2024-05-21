#pragma once

#include <sensors/sensors.h>
#include <cassert>
#include <iostream>
#include <vector>

struct libsensors_item {
  std::string chip_path;
  sensors_chip_name const* chip_name;
  std::string feature_name;
  int feature_num;
  const sensors_feature_type feature_type;
  std::string subfeature_name;
  int subfeature_num;
  const sensors_subfeature_type subfeature_type;
};

class SensorList {
 public:
  std::vector<libsensors_item> sensors;

  SensorList() {
    sensors_chip_name const* chip;
    int c = 0;

    // get a chip
    while ((chip = sensors_get_detected_chips(0, &c)) != NULL) {
      assert(sensors_snprintf_chip_name(nullptr, 0, chip) > 0);

      sensors_feature const* feature;
      int f = 0;

      // get a feature of said chip
      while ((feature = sensors_get_features(chip, &f)) != NULL) {
        auto label{sensors_get_label(chip, feature)};

        sensors_subfeature const* subfeature;
        int s = 0;

        // get subfeature of said feature = file like temp1_input
        while ((subfeature = sensors_get_all_subfeatures(chip, feature, &s)) !=
               NULL) {
          auto name = std::string(subfeature->name);

          libsensors_item this_item = {
            .chip_path = chip->path,
            .chip_name = chip,
            .feature_name = feature->name,
            .feature_num = static_cast<int>(feature->number),
            .feature_type = feature->type,
            .subfeature_name = subfeature->name,
            .subfeature_num = static_cast<int>(subfeature->number),
            .subfeature_type = subfeature->type};

          // put all knwown sensors into vector
          sensors.push_back(this_item);
        }
      }
    }
  }

  void checknosensors() {
    if (sensors.size() == 0) {
      throw std::runtime_error("no sensors found");
    }
  }

  // outputs all sensors with features and subfeatures
  void outputSensorList() {
    std::cout << "full_path;chip_path;subfeature;feature\n";

    for (const auto& chip_item : sensors) {
      std::string full_path =
          chip_item.chip_path + "/" + chip_item.subfeature_name;

      std::cout << full_path << ";" << chip_item.chip_path << "/;"
                << chip_item.subfeature_name << ";" << chip_item.feature_name
                << "\n";
    }
  }
};

int printSensorList() {
  // has to be called for sensors to be found
  sensors_init(nullptr);

  SensorList list;

  list.outputSensorList();

  return 0;
}

/*
for future reference:

chip path = /sys/class/hwmon/hwmon5

feature name = temp1
feature number and type are internally used vars and (probaply) do not
correspond to actual sensors

subfeature name = temp1_input
subfeature number and type are internally used vars and (probaply) do not
correspond to actual sensors
*/
