set shell := ["bash", "-eEuo", "pipefail", "-c"]

BUILD_DIR := "build-out-debug-clang"

default:
    @just --list

buildall: && build
    CXX=clang++ CC=clang cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -H. -DGOOGLE_CLOUD_CPP_ENABLE=bigtable -B{{ BUILD_DIR }}
    ln -sf {{ BUILD_DIR }}/compile_commands.json .

build:
    make -C {{ BUILD_DIR }} -j`nproc`

test:
    for f in {{ BUILD_DIR }}/google/cloud/bigtable/emulator/*_test ; do echo === $f ===; $f || break ; done

testall:
    for f in {{ BUILD_DIR }}/google/cloud/bigtable/emulator/*_test ; do echo === $f ===; $f || true ; done

clangtidy:
    ./ci/cloudbuild/build.sh -t clang-tidy-ci

checkersci:
    ./ci/cloudbuild/build.sh -t checkers-ci

servertest:
    make -C {{ BUILD_DIR }} -j`nproc` bigtable_emulator_server_test/fast
    {{ BUILD_DIR }}/google/cloud/bigtable/emulator/server_test

filtertest:
    rm {{ BUILD_DIR }}/google/cloud/bigtable/emulator/filter_test || true
    make -C {{ BUILD_DIR }} -j`nproc` bigtable_emulator_filter_test/fast
    {{ BUILD_DIR }}/google/cloud/bigtable/emulator/filter_test

# export GTEST_FILTER := "FilterIntegrationTest.ColumnRange"

brian:
    google/cloud/bigtable/ci/run_integration_tests_emulator_cmake.sh {{ BUILD_DIR }} -Lintegration-test-emulator --timeout 60

brianlog:
    less {{ BUILD_DIR }}/Testing/Temporary/LastTest.log

brianlog2:
    less {{ BUILD_DIR }}/emulator.log