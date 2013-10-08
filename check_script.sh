#!/bin/bash

echo "okay"
SERVICE=$1
echo "okay"
echo ${SERVICE}
RESULT=`ps -eo pid | grep ${SERVICE}`
if [ "${RESULT}" = ${SERVICE} ]; then
	echo "running"
else
	echo "not running"
fi
