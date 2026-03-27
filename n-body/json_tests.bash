#!/usr/bin/env bash

if [ ! -d json_examples ]; then
    echo "Can't find examples folder for testing"
    exit 1
fi

TEST_EXAMPLES=(json_examples/*.json)

for example in "${TEST_EAMPLES[@]}"; do
    ./bin/json-test "$example"
done
