#!/usr/bin/env bash

set -xe

export PGHOME=/pg
export PG_SRC=$PWD/postgres
export PATH=$PGHOME/bin:$PATH
export LD_LIBRARY_PATH=$PGHOME/lib
export PG_CONFIG=$(which pg_config)

# Build and install pg_probackup (using PG_CPPFLAGS and SHLIB_LINK for gcov)
echo "############### Compiling and installing pg_probackup:"
# make USE_PGXS=1 PG_CPPFLAGS="-coverage" SHLIB_LINK="-coverage" top_srcdir=$CUSTOM_PG_SRC install
make USE_PGXS=1 top_srcdir=$PG_SRC install

echo "############### Testing:"
#echo PG_PROBACKUP_PARANOIA=${PG_PROBACKUP_PARANOIA}
#echo ARCHIVE_COMPRESSION=${ARCHIVE_COMPRESSION}
#echo PGPROBACKUPBIN_OLD=${PGPROBACKUPBIN_OLD}
#echo PGPROBACKUPBIN=${PGPROBACKUPBIN}
#echo PGPROBACKUP_SSH_REMOTE=${PGPROBACKUP_SSH_REMOTE}
#echo PGPROBACKUP_GDB=${PGPROBACKUP_GDB}
#echo PG_PROBACKUP_PTRACK=${PG_PROBACKUP_PTRACK}

export PG_PROBACKUP_TEST_BASIC=ON
python3 -m unittest -v tests
