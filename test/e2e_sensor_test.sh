#!/usr/bin/env bash
set -euo pipefail


#
# does all tests with sensors, also tests nullreader
# this isn't included in the pipeline, as it doesn't have access to sensors
#

# finds all output files and deletes them
function delete_output () {
    find $DIR -name '*_timestamp_value.csv' -delete -o -name '*_duration_value.csv' -delete -o -name 'metadata.toml' -delete
}

# get hwmondump binary from console input
HWMONDUMP_BIN=$1

test -f $HWMONDUMP_BIN -a -x $HWMONDUMP_BIN || (echo "hwmondump binary not found at: $HWMONDUMP_BIN" >&2 && exit 127)

TEST_SENSOR=$("$HWMONDUMP_BIN" list | \
                tail -n+2 | \
                head -n1 | \
                cut -d ";" -f 1 | \
                tr -d "\0")
echo $TEST_SENSOR

# make temp dir
DIR=$(mktemp -d)

# delete said temp dir on close
function cleanup()
{
    echo "exit code:    " $?
    rm -r $DIR
}
trap cleanup EXIT SIGINT SIGTERM

# go to temp dir for simpler tests
cd $DIR

# should work
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --sysfs -a 100
test -f ./metadata.toml
test -f ./sysfs_timestamp_value.csv
test -f ./sysfs_duration_value.csv
test '!' -f ./lseek_timestamp_value.csv
test '!' -f ./lseek_duration_value.csv
test '!' -f ./libsensors_timestamp_value.csv
test '!' -f ./libsensors_duration_value.csv
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv
delete_output

# lowering accessnum from here on out, simply to allow e2e tests to
# finish within a humans lifespan *wink*

"$HWMONDUMP_BIN" record "$TEST_SENSOR" --sysfs --sysfs-lseek -a 100
test -f ./metadata.toml
test -f ./sysfs_timestamp_value.csv
test -f ./sysfs_duration_value.csv
test -f ./lseek_timestamp_value.csv
test -f ./lseek_duration_value.csv
test '!' -f ./libsensors_timestamp_value.csv
test '!' -f ./libsensors_duration_value.csv
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv
delete_output

"$HWMONDUMP_BIN" record "$TEST_SENSOR" --sysfs --sysfs-lseek --libsensors -a 100
test -f ./metadata.toml
test -f ./sysfs_timestamp_value.csv
test -f ./sysfs_duration_value.csv
test -f ./lseek_timestamp_value.csv
test -f ./lseek_duration_value.csv
test -f ./libsensors_timestamp_value.csv
test -f ./libsensors_duration_value.csv
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv
delete_output

"$HWMONDUMP_BIN" record "$TEST_SENSOR" --sysfs --sysfs-lseek --libsensors --null -a 100
test -f ./metadata.toml
test -f ./sysfs_timestamp_value.csv
test -f ./sysfs_duration_value.csv
test -f ./lseek_timestamp_value.csv
test -f ./lseek_duration_value.csv
test -f ./libsensors_timestamp_value.csv
test -f ./libsensors_duration_value.csv
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
delete_output

# note: cleanup by trap
