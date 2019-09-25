#!/bin/bash

echo "preparation script"

pwd

supervisorctl status all
supervisorctl update all
sleep 3
supervisorctl status all

/builds/iopsys/owsd/build/owsd -f /builds/iopsys/owsd/build/test/cmocka/files/tmp/owsd/rpt_cfg.json  > /dev/null 2&>1  &

#report part
#GitLab-CI output
make functional-coverage -C ./build &

sleep 2

make functional-test -C ./build

# hacky way to kill the functional-coverage gracefully

ps -ax

sleep 1

kill $(pidof owsd)

supervisorctl stop all
supervisorctl status

tar -zcvf functional-coverage.tar.gz ./build/functional-coverage
