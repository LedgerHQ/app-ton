#!/bin/bash -eu

# build fuzzers

pushd fuzzing
cmake -DBOLOS_SDK=../BOLOS_SDK -DTARGET=nanos -Bbuild -H.
make -C build
mv ./build/fuzz_tx_parser "${OUT}"
popd