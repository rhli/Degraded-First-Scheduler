#!/bin/bash

test=test
test_bin=bin

cd $test
make clean
make
cd -

for file in $test_bin/*
do
  if [[ -x "$file" ]]
  then
    $file
    read -p "TEST $file done. Press enter to run next set"
  fi
done
