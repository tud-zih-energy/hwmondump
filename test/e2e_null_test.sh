#!/usr/bin/env bash
set -euo pipefail

#
# does all tests that are possible without sensors, using the nullreader
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

# missing subcommands
! "$HWMONDUMP_BIN"

# should work
"$HWMONDUMP_BIN" list

# missing sensor path
! "$HWMONDUMP_BIN" record

#no method
! "$HWMONDUMP_BIN" record "$TEST_SENSOR"


test '!' -f ./metadata.toml
test '!' -f ./sysfs_timestamp_value.csv
test '!' -f ./sysfs_duration_value.csv
test '!' -f ./lseek_timestamp_value.csv
test '!' -f ./lseek_duration_value.csv
test '!' -f ./libsensors_timestamp_value.csv
test '!' -f ./libsensors_duration_value.csv
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# -a (--accessnum) is not required
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null
test -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
delete_output

# lowering accessnum from here on out, reason see e2e_sensor_test.sh

# should work
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 100
test -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
test '!' -f ./sysfs_timestamp_value.csv
test '!' -f ./sysfs_duration_value.csv
test '!' -f ./lseek_timestamp_value.csv
test '!' -f ./lseek_duration_value.csv
test '!' -f ./libsensors_timestamp_value.csv
test '!' -f ./libsensors_duration_value.csv
delete_output

# duplicate argument
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null --null -a 100
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# skip metadata
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null --no-metadata -a 100
test '!' -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
delete_output

# directory for output
mkdir "$DIR"/o/

# should work
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -o ./o/ -a 100
test -f ./o/metadata.toml
test -f ./o/null_timestamp_value.csv
test -f ./o/null_duration_value.csv
delete_output

"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null --output ./o/ -a 100
test -f ./o/metadata.toml
test -f ./o/null_timestamp_value.csv
test -f ./o/null_duration_value.csv
delete_output

# will currently fail, should work tho
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -o ./o -a 100
test -f ./o/metadata.toml
test -f ./o/null_timestamp_value.csv
test -f ./o/null_duration_value.csv
delete_output

# should work
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 10
test -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
delete_output

# accessnum too small
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 5
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# accessnum too small (negative)
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a -102
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# accessnum too small, 0 ignored
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 09
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv
delete_output

# timed access
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -t 2
test -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
grep -v 'accessnum' metadata.toml > /dev/null
grep -E 'accesstime_s *= *2' metadata.toml > /dev/null
delete_output

# accesstime is optional
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null
grep -v 'accessnum' metadata.toml > /dev/null
grep 'accesstime_s' metadata.toml > /dev/null
delete_output

# should work, 0 ignored
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -t 01
test -f ./metadata.toml
test -f ./null_timestamp_value.csv
test -f ./null_duration_value.csv
delete_output

# accesstime too small
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -t 0
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# accesstime too small (negative)
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -t -102
test '!' -f ./metadata.toml
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv

# should work
"$HWMONDUMP_BIN" --help
"$HWMONDUMP_BIN" list --help
"$HWMONDUMP_BIN" record --help
"$HWMONDUMP_BIN" analysis --help

# output file already exists
touch null_timestamp_value.csv
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 100
test '!' -f ./metadata.toml
delete_output

# metadata file already exists
touch metadata.toml
! "$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -a 100
test '!' -f ./null_timestamp_value.csv
test '!' -f ./null_duration_value.csv
delete_output

# no files
! "$HWMONDUMP_BIN" analysis --median

# make files to analyze
echo duration,value > ./mock1_timestamp_value.csv
echo 20,42 >> ./mock1_timestamp_value.csv
echo 24,42 >> ./mock1_timestamp_value.csv

# missing analysis goal (e.g. --median)
! "$HWMONDUMP_BIN" analysis

# should work
"$HWMONDUMP_BIN" analysis --median

# create multiple output files
echo duration,value > ./mock2_timestamp_value.csv
echo 2,42 >> ./mock2_timestamp_value.csv
echo 15,42 >> ./mock2_timestamp_value.csv

"$HWMONDUMP_BIN" analysis --median

#should work
"$HWMONDUMP_BIN" analysis --median -d ./
"$HWMONDUMP_BIN" analysis --median --directory ./

# create new directory
test '!' -d newdir
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -o ./newdir/here/ -a 100
test -d newdir
test -d newdir/here

test '!' -d newerdir
"$HWMONDUMP_BIN" record "$TEST_SENSOR" --null -o ./newerdir/here -a 100
test -d newerdir/here

# analysis as csv output
LINECOUNT=$("$HWMONDUMP_BIN" analysis --csv-header -d doesnotexist_but_doesnt_matter | wc -l)
test 1 -eq "$LINECOUNT"

LINECOUNT=$("$HWMONDUMP_BIN" analysis -d newdir/here --median --csv | wc -l)
test 1 -eq "$LINECOUNT"
rm -r newdir
rm -r newerdir

# note: cleanup by trap
