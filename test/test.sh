#!/usr/bin/env bash
set -ue

sleep 5

# import sub_test.sh
$(dirname $0)/sub_test.sh

echo 'finished!!'
