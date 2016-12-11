#!/bin/bash

#Run with redundancy=0
iquery -anq "remove(test_array_1)" > /dev/null 2>&1
iquery -anq "remove(test_array_2)" > /dev/null 2>&1

iquery -anq "create_with_residency(test_array_1, <val:double> [x=1:40:0:10], false, 'instances=0')" > test.out
iquery -anq "store(build(test_array_1, x), test_array_1)" >> test.out
iquery -anq "create array test_array_2 <val:double> [x=1:40:0:10]" >> test.out
iquery -anq "store(build(test_array_2, x), test_array_2)" >> test.out

#NOTE: tried not to make it depend on equi_join and summarize
#Should say 8 chunks on instance 0
iquery -otsv+ -aq "aggregate(filter(cross_join(list('chunk map') as A, filter(list('arrays'), name='test_array_1') as B), A.uaid=B.uaid), count(*), inst)" >> test.out
iquery -otsv+ -aq "op_count(join(test_array_1, test_array_2))" >> test.out
iquery -otsv+ -aq "op_count(filter(join(test_array_1, test_array_2), test_array_1.val <> test_array_2.val))" >> test.out

diff test.out test.expected
