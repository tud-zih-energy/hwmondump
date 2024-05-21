# hwmondump - reading hwmon sensor data
`hwmondump` can output a list of all available sensors on your system, read sensor values in three seperate ways and analyse this output. This can be used to compare the overhead of each method or simply monitor a sensor.
The following readout methods are available:

- `sysfs`
    - uses hwmon-files from `/sys/class/hwmon/`
    - reopens the file with `open()` syscall for every read
    - procedure: Open - Read - Close - Open - Read - Close - ...
- `sysfs-lseek`
    - also uses hwmon-files from `/sys/class/hwmon/`
    - only uses the `open()` syscall once, then calls `lseek()` for all subsequent readouts
    - procedure: Open - Read - Seek - Read - ... - Close
- `libsensors`
    - calls on libsensors library
- `null` (for testing purposes)
    - doesn't read sensor data -- always reports `0`
    - can be used to benchmark the speed of hwmondump itself

You can specify how often this access should happen and where the generated csv and toml files should be saved.

## Installation
### Build Requirements
- lm-sensors
- libcpuid
- CMake (>=3.24)
- A C++ Compiler with C++17 support and the `std::filesystem` library
- Catch2 if you want to enable testing

### Build Process
Use the standard CMake procedure:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install # optional
```

The program will be installed as `hwmondump`.

> It is recommended to build with optimizations enabled (via `-DCMAKE_BUILD_TYPE=Release`) to avoid unnecessary overhead during readouts.

### Testing
You can enable testing by calling `ccmake ..` from the build directory and switching `BUILD_TESTING` to `ON`.

> Don't forget to configure and generate before closing

You can now run unittests and end-to-end-tests with `make test`. Note that e2e_sensor_test might not pass in your pipeline, as hwmondump won't have any sysfs files to access.

## Quick Start: Usage
### List Sensors
```
$ hwmondump list
full_path;chip_path;subfeature;feature
/sys/class/hwmon/hwmon6/fan1_input;/sys/class/hwmon/hwmon6/;fan1_input;fan1
/sys/class/hwmon/hwmon6/temp1_input;/sys/class/hwmon/hwmon6/;temp1_input;temp1
/sys/class/hwmon/hwmon6/temp2_input;/sys/class/hwmon/hwmon6/;temp2_input;temp2

# ...then start a measurement with the last sensor
$ hwmondump record --sysfs-lseek --libsensors -a 1500 -o ~/result_dir/ /sys/class/hwmon/hwmon6/temp2_input
```

The first column of each output row contains the path to each sensor. The other rows are what libsensors considers a chip, a subfeature and a feature. (You can safely ignore this for simple usage.)

> The first column is a valid path in the sysfs.
> You can forego `hwmondump list` entirely and browse the sysfs yourself.

### Run a benchmark
Use the sensor path you just chose to run a benchmark with any combination of methods. The call for this looks something like this:

```
$ hwmondump record --sysfs-lseek --libsensors -a 1500 -o ~/result_dir/ /sys/class/hwmon/hwmon6/temp2_input
Path: /sys/class/hwmon/hwmon6/temp1_input
[lseek] starting benchmark...
        Time Estimate:     11.1606 ms
        Real Runtime:      10.436 ms

[lseek] postprocessing...
[lseek] saving...
[lseek] done

[libsensors] starting benchmark...
        Time Estimate:     10.4124 ms
        Real Runtime:      10.0621 ms

[libsensors] postprocessing...
[libsensors] saving...
[libsensors] done
```

The output will be in `~/result_dir` (if the directory did not exist prior to calling `hwmondump record` it will first be created):

```
$ ls -lh ~/result_dir
total 104K
-rw-r--r-- 1 alice users  72 Feb 19 18:05 libsensors_duration_value.csv
-rw-r--r-- 1 alice users 44K Feb 19 18:05 libsensors_timestamp_value.csv
-rw-r--r-- 1 alice users  18 Feb 19 18:05 lseek_duration_value.csv
-rw-r--r-- 1 alice users 49K Feb 19 18:05 lseek_timestamp_value.csv

$ cat libsensors_timestamp_value.csv
nanoseconds,value
1708412937309530755,38625.000000
1708412937309539962,38625.000000
1708412937309547446,38625.000000
1708412937309555161,38625.000000
1708412937309563016,38625.000000
[...]

$ cat libsensors_duration_value.csv
nanoseconds,value
11998493,38750.000000
9609527,38875.000000
9589920,39000.000000
10812796,39125.000000
7240317,39250.000000
[...]
```

> There are two equally right or wrong ways to calculate the duration, which yield different results.
> `hwmondump` assumes a value change happens right after (quasi-instantly) a new value is recorded.
> For this reason, the first recorded value (`38625.000000`) doesn't show up in the `libsensors_duration_value.csv` file.

### Analyze your collected data
You can now calculate the median of your recording. To start the analysis, type this:
```
$ hwmondump analysis --median
lseek: 7026303 nanoseconds
libsensors: 6793476 nanoseconds
```
No files will be created through this command.

## Output Format
`hwmondump record` produces two csv files per recorded method.
They will be stored in a directory given by `-o`/`--output` (default: current working directory).

- `[METHOD]_timestamp_value.csv`:
   **Timestamp** in nanoseconds of each sensor access and the value which the sensor had at the time.
- `[METHOD]_duration_value.csv`:
  **Duration** in nanoseconds for which a sensor value was recorded and (of course) which value that was.
- `metadata.toml`:
  **Metadata** for each time you start a benchmark, see manpage for more information

Existing files will not be overwritten.

## Additional Documentation
For more information on the usage of `hwmondump` see:

- `hwmondump --help`, also for its subcommands:
  - `hwmondump record --help`
  - `hwmondump list --help`
  - `hwmondump analysis --help`
- `man hwmondump`
