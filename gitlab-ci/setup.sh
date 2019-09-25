#!/bin/bash

echo "preparation script"

pwd

adduser --disabled-password --gecos "" user
echo "user:user"|chpasswd

mkdir build
pushd build
	cmake .. -DWSD_HAVE_UBUS=ON -DOWSD_UPROXYD=ON -DCMAKE_BUILD_TYPE=Debug -DOWSD_JSON_VALIDATION=ON
	make
	make install
popd
cp -r ./test/cmocka/files/* /

cp ./gitlab-ci/iopsys-supervisord.conf /etc/supervisor/conf.d/
