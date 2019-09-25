#!/bin/bash

echo "preparation script"

pwd

make unit-test -C ./build


#report part
#GitLab-CI output
make unit-coverage -C ./build

tar -zcvf unit-coverage.tar.gz ./build/unit-coverage
