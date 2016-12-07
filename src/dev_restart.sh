#!/bin/bash
#Quick restart script for dev use
iquery -aq "unload_library('variable_residency')" > /dev/null 2>&1
set -e

DBNAME="mydb"
SCIDB=`which scidb`
SCIDB_INSTALL="`dirname $SCIDB`/.."

export SCIDB_THIRDPARTY_PREFIX="/opt/scidb/16.9"

mydir=`dirname $0`
pushd $mydir
make SCIDB=$SCIDB_INSTALL

scidb.py stopall $DBNAME 
cp libvariable_residency.so ${SCIDB_INSTALL}/lib/scidb/plugins/
scidb.py startall $DBNAME 
iquery -aq "load_library('variable_residency')" 
