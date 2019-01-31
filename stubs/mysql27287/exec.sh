#!/usr/bin/env bash

./mysql_install_db  --defaults-file=./my-medium.cnf.sh

"$1" --defaults-file=./my-medium.cnf.sh &

sleep 2

./mysql --defaults-file=./my-medium.cnf.sh  -u root test < "$2"

./mysql --defaults-file=./my-medium.cnf.sh -u root test < ./0.sql

pkill -f "$1"

sleep 2
