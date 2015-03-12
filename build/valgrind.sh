#!/bin/bash

# NOTE: use nc 127.0.0.1 9999 to connecto to the server and write/read a message.
# NOTE: you can create any number of "nc" clients to test the pool.

#valgrind --tool=memcheck --leak-check=yes --log-file=./memcheck.log ./tcp_server 9999 10
valgrind --tool=helgrind --log-file=./helgrind.log ./tcp_server 9999 10
#valgrind --tool=drd --log-file=./drd.log ./tcp_server 9999 10
