#!/bin/bash

cp test/calvin.confLOCAL calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt1 -n 127.0.0.1 -p 5000 -c 5001 &
RT1_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5001
exit_code+=$?
rm calvin.conf

cp test/calvin.confPROXY calvin.conf
PYTHONPATH=calvin-base python calvin-base/calvin/Tools/csruntime.py --name rt2 -n 127.0.0.1 -p 5002 -c 5003 &
RT2_PID=$!
PYTHONPATH=calvin-base python test/verify_runtime.py http://127.0.0.1:5003
exit_code+=$?
rm calvin.conf

# Run dmce
../dmce/dmce-launcher -n 2

# build and start calvin-constrained
make -f platform/x86/Makefile
exit_code+=$?
./calvin_c -n constrained -p "calvinip://127.0.0.1:5000" 2> cc_stderr.log &
CONSTRAINED_RT_PID=$!

# run test
PYTHONPATH=calvin-base py.test -sv test/test.py
exit_code+=$?

../dmce/dmce-summary cc_stderr.log
# clean up
kill -9 $CONSTRAINED_RT_PID
kill -9 $RT1_PID
kill -9 $RT2_PID
rm calvinconstrained.config
cd calvin-base
git checkout calvin/csparser/parsetab.py

exit $exit_code
