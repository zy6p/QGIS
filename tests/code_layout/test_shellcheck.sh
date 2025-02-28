#!/usr/bin/env bash

# This runs shellcheck given or found sh files

FILES_TO_CHECK="$@"

DIR=$(realpath $(dirname $0)/../../)

pushd ${DIR} > /dev/null || exit

if test -z "$FILES_TO_CHECK"; then
  FILES_TO_CHECK=$(find . -name '*.sh')
fi

echo "Running shell check on files: $FILES_TO_CHECK"

result=$(shellcheck -e SC2016,SC2015,SC2086,SC2002,SC1117,SC2154,SC2076,SC2046,SC1090,SC2038,SC2031,SC2030,SC2162,SC2044,SC2119,SC1001,SC2120,SC2059,SC2128,SC2005,SC2013,SC2027,SC2090,SC2089,SC2124,SC2001,SC2010,SC1072,SC1073,SC1009,SC2166,SC2045,SC2028,SC1091,SC1083,SC2021 ${FILES_TO_CHECK})
popd > /dev/null || exit

if [[ $result ]]; then
  echo " *** shellcheck found script errors"
  echo "$result"
  exit 1
fi
