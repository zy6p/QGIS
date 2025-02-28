#!/bin/sh

srcdir=$(dirname $0)/..

command -v git > /dev/null || {
  echo "git is not found in PATH" >&2
  exit 2 # considered a "skip"
}

if test "${QGIS_TEST_ACCEPT_GITSTATUS_CHECK_FAILURE}" = "1"; then
	exit=2
else
	exit=1
fi

git -C ${srcdir} status -uno > .gitstatus-full || exit $exit

if test "$1" = "log"; then
  grep 'modified: ' .gitstatus-full | sort -u > .gitstatus
  exit 0
elif test "$1" = "check"; then
  if grep 'modified: ' .gitstatus-full | sort -u | diff - .gitstatus; then
    exit 0
  else
    echo "Source files (printed above) were modified. Diff follows:"
    git -C ${srcdir} diff
    exit $exit
  fi
else
  echo "Usage: $0 [log|check]" >&2
  exit 1
fi
