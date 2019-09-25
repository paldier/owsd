#!/bin/bash

echo "preparation script"

pwd

supervisorctl status all
supervisorctl update all
sleep 3
supervisorctl status all

/builds/iopsys/owsd/build/owsd -f /builds/iopsys/owsd/build/test/cmocka/files/tmp/owsd/rpt_cfg.json  > /dev/null 2&>1  &

make functional-api-coverage -C ./build &

# let ubus-proxy be processed
sleep 1

ubus-api-validator -d test/api/json

kill $(pidof owsd)
supervisorctl stop all
supervisorctl status

tar -zcvf functional-api-coverage.tar.gz ./build/functional-api-coverage
