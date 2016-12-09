# variable_residency
Prototypes and notes on Variable Array Residency in SciDB.

![image](https://cloud.githubusercontent.com/assets/2708498/21032954/28b62904-bd7b-11e6-886f-971c7e74d768.png)

Starting with 15.12, core SciDB is made aware of the concept of _residency_, that is the set of instances that chunks of an array are persited on. By default, chunks are hash-distributed across all the processes in the cluster, and the residency is said to _include all the instances_. In most cases that is desired to achieve the most parallelism. But here we begin to explore breaking that rule - restricting some or all arrays to reside only a _subset_ of the instances. There are many possible use cases, such as heterogeneous clusters with different storage media (hot and cold data that can be joined together), or, perhaps, limiting which CPU cores will be used to query which arrays.

The elasticity feature in the Enterprise Edition allows us to do even more interesting workflows, such as:

1. start with a cluster of K instances
2. attach M additional "compute" instances
3. use M+K instances to run a heavy computational workload
4. save the computation result onto the original K instances
5. detach the "compute" instances and return to the original K

This is particularly attractive in cloud environments: compute nodes can be rented on an hourly basis, only for when they are needed, then discarded. Cloud costs become a lot more attractive than in-house storage. This also implies that any SciDB cluster can be made to 'grow' or 'shrink' as demand changes.

This repository contains a small plugin `variable_residency` with a single operator `create_with_residency`. We first discuss the operator, then show an example 'grow, compute, shrink' workflow that EE enables.

# 1. Operator create_with_residency

The plugin installs using [dev_tools](https://github.com/paradigm4/dev_tools) for 15.12 and 16.9. Make sure you use the `v15.12` branch for 15.12

`create_with_residency` is a slightly modified `create_array` that allows the user to specify a set of instances. The syntax is as follows:
`create_with_residency( ARRAY_NAME, SCHEMA, TEMP, INSTANCE0 [, INSTANCE1 [, INSTANCE2,...])` where

* `ARRAY_NAME` and `SCHEMA` are just as in `create_array`
* `TEMP` is a boolean flag (true is equivalent to `create temp array`)
* `INSTANCE0...N` are uint64 instance identifiers as displayed in `list('instances')`

## Example

We have a 1-node 16-instance SciDB installation. Let's create an array that may only reside on instances 3 and 4:
```bash
$ iquery -aq "op_count(list('instances'))"
{i} count
{0} 16

$ iquery -aq "create_with_residency(foo, <val:double>[x=1:40,10,0], false, 3, 4)"
Query was executed successfully

$ iquery -anq "store(build(foo, x), foo)"
Query was executed successfully
```
The array has 4 10-element chunks. Normally those chunks would land on unique instances in a 16-instance cluster, but in this case we restricted the residency to just 3 and 4. We can use [summarize](https://github.com/paradigm4/summarize) to confirm that, indeed, the residency is reduced:
```bash
$ iquery -aq "summarize(foo, 'per_instance=1')"
{inst,attid} att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes
{0,0} 'all',0,0,0,null,null,null,null,null,null
{1,0} 'all',0,0,0,null,null,null,null,null,null
{2,0} 'all',0,0,0,null,null,null,null,null,null
{3,0} 'all',20,400,4,10,10,10,48,100,152
{4,0} 'all',20,400,4,10,10,10,48,100,152
{5,0} 'all',0,0,0,null,null,null,null,null,null
{6,0} 'all',0,0,0,null,null,null,null,null,null
{7,0} 'all',0,0,0,null,null,null,null,null,null
...
```
Compare that to a "regular" array with the same shape and data:
```bash
$ iquery -aq "create array bar <val:double>[x=1:40,10,0]"
Query was executed successfully

$ iquery -anq "store(build(bar, x), bar)"
Query was executed successfully

$ iquery -aq "summarize(bar, 'per_instance=1')"
{inst,attid} att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes
{0,0} 'all',10,200,2,10,10,10,48,100,152
{1,0} 'all',10,200,2,10,10,10,48,100,152
{2,0} 'all',10,200,2,10,10,10,48,100,152
{3,0} 'all',10,200,2,10,10,10,48,100,152
{4,0} 'all',0,0,0,null,null,null,null,null,null
{5,0} 'all',0,0,0,null,null,null,null,null,null
{6,0} 'all',0,0,0,null,null,null,null,null,null
{7,0} 'all',0,0,0,null,null,null,null,null,null
...
```

Most importantly, even though the two arrays differ in residency, the SciDB optimizer and/or specific operators are smart enough to redistribute the data when the arrays need to be used together in a query:
```bash
$ iquery -aq "join(foo,bar)"
{x} val,val
{1} 1,1
{2} 2,2
{3} 3,3
{4} 4,4
{5} 5,5
{6} 6,6
{7} 7,7
{8} 8,8
{9} 9,9
...
```

## Residency Lifetime

At the moment, the residency is associated with an array name at creation time and cannot be mutated. Thus, storing or inserting something into `foo` will not change its residency:
```bash
$ iquery -aq "insert(project(apply(between(bar, 1,3), val2, val*10), val2), foo)"
{x} val
{1} 10
{2} 20
{3} 30
{4} 4
{5} 5
{6} 6
{7} 7
{8} 8
{9} 9
...

$ iquery -aq "summarize(foo, 'per_instance=1')"
{inst,attid} att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes
{0,0} 'all',0,0,0,null,null,null,null,null,null
{1,0} 'all',0,0,0,null,null,null,null,null,null
{2,0} 'all',0,0,0,null,null,null,null,null,null
{3,0} 'all',20,400,4,10,10,10,48,100,152
{4,0} 'all',20,400,4,10,10,10,48,100,152
{5,0} 'all',0,0,0,null,null,null,null,null,null
{6,0} 'all',0,0,0,null,null,null,null,null,null
{7,0} 'all',0,0,0,null,null,null,null,null,null
...
```

Similarly, storing `foo` into a new array requires creating a new array with default residency:
```bash
$ iquery -naq "store(foo, foo2)"
Query was executed successfully

$ iquery -aq "summarize(foo2, 'per_instance=1')"
{inst,attid} att,count,bytes,chunks,min_count,avg_count,max_count,min_bytes,avg_bytes,max_bytes
{0,0} 'all',10,200,2,10,10,10,48,100,152
{1,0} 'all',10,200,2,10,10,10,48,100,152
{2,0} 'all',10,200,2,10,10,10,48,100,152
{3,0} 'all',10,200,2,10,10,10,48,100,152
{4,0} 'all',0,0,0,null,null,null,null,null,null
{5,0} 'all',0,0,0,null,null,null,null,null,null
{6,0} 'all',0,0,0,null,null,null,null,null,null
{7,0} 'all',0,0,0,null,null,null,null,null,null
...
```

This implies that all arrays with non-default residency must be created first, before they are inserted into.

## Redundancy

If SciDB replication is enabled, 

# 2. The grow, compute, shrink workflow

These steps require the P4 Enterprise Edition:
```bash
$ iquery -aq "load_library('system')"
Query was executed successfully
```

For the sake of the example, we perform the following steps:

 1. start with 1 node, 16 instances
 2. create a test dataset and show a sample query on it
 3. dynamically attach a second node with 16 more instances
 4. use all 32 instances to run the query in less time, save the result 
 5. detach the second node returning to the original 1-node setup
 6. SciDB remains operational the original dataset and the computed query result are retained as arrays
 
The example is quite simple on purpose. In practice, you can attach more than one node at once, and the analyses can be much more involved. All the commands run on the initial node 0.
 
## 2.1 Test Data and Query

What we really have in mind is an complex workflow, perhaps a monte-carlo simulation driven by [stream](https://github.com/Paradigm4/stream). But we'll use a very simple example for brevity. We create a vector of 600M numbers between 0 and 19:
```bash
$ iquery -anq "store(build(<val:double> [i=1:600000000,1000000,0], random()%20), test_array)"
Query was executed successfully
```
This creates default residency; the array is hashed across all 16 instances. Our test query will use [grouped_aggregate](https://github.com/Paradigm4/grouped_aggregate) to quickly count the occurrence of each number, like so:
```bash
$ time iquery -otsv -aq "grouped_aggregate(test_array, count(*), val)"
4       29989190
0       30000877
10      30009388
6       30001146
17      30002570
15      29993610
1       29995601
8       30006619
7       29997761
11      29996032
5       30001637
13      30002346
16      29994724
18      30000196
19      30000504
12      29998716
3       29993538
14      30000911
2       30006660
9       30007974

real    0m13.643s
user    0m0.006s
sys     0m0.006s
```
As expected, the values are evenly distributed. 

## 2.2 What the dynamically attached nodes need
Attaching another node takes at most 1-2 minutes. It may take some time at first to ensure the second node has all the right packages installed and set up. Luckily, cloud tools lke "saved machine images" allow you to first configure a system and then save it for reuse. Specifically the second node needs the following:

 * same version of SciDB 
 * all of the same .so plugin files (/opt/scidb/VER/lib/scidb/plugins)
 * ssh key for the user `scidb` so that password-less ssh is possible
 * the .pgpass file 
 * same data directory layout as the first node (needs to have some space at least for logs, etc)
 
Ports also to be open between the nodes: 

 * 5432 for Postgres 
 * 1239-... for SciDB instances
 * additional ports for linear algebra
 
It might be best to place the nodes on a private network with all node-to-node ports open. Postgres may also need to be configured to allow and trust connections for remote IPs. Finally, the `config.ini` file for Node 1 must mention the exact IP or hostname - do not use `localhost` or `127.0.0.1` as that will confuse a two-node setup.

## 2.3 Attaching the second node
We first create a "config.ini" delta file. It needs to contain the same installation name (`mydb` in our case), the IP address(es) of the new nodes and the desired number of instances per node, like so:
```bash
$ cat config-add-delta.ini
[mydb]
server-1 = 10.139.99.10,15
```
Note `server-1` is the "second" server. We are already presumed to be running `server-0`. You may also place `data-dir-prefix` lines in this file to set up multiple disks.

### 2.3.1 Register new instances
We use the `config_server` tool to register the new instances with the catalog:
```bash
$ scidb.py -m p4_system config_server --add config-add-delta.ini --output new_config.ini mydb /opt/scidb/16.9/etc/config.ini
```
Note this has the effect of taking the existing `config.ini`, changing it and updating it to `new_config.ini` as specified. We will hold on to this file for a few future commands.

This also has the effect of ssh-ing into the second node, initializing the data directories and registering the new instances with the catalog. After this step, the EE `list_instances()` command (not to be confused with `list('instances')`) will display the new instances as `registered`:
```bash
$ iquery -aq "list_instances()"
{No} instance_id,membership,member_since,liveness,server_id,server_instance_id,host,port,base_path
{0} 0,'member','2016-12-08 22:35:59',true,0,0,'10.146.19.22',1239,'/home/scidb/data'
{1} 1,'member','2016-12-08 22:35:59',true,0,1,'10.146.19.22',1240,'/home/scidb/data'
{2} 2,'member','2016-12-08 22:35:59',true,0,2,'10.146.19.22',1241,'/home/scidb/data'
{3} 3,'member','2016-12-08 22:35:59',true,0,3,'10.146.19.22',1242,'/home/scidb/data'
{4} 4,'member','2016-12-08 22:35:59',true,0,4,'10.146.19.22',1243,'/home/scidb/data'
{5} 5,'member','2016-12-08 22:35:59',true,0,5,'10.146.19.22',1244,'/home/scidb/data'
{6} 6,'member','2016-12-08 22:36:00',true,0,6,'10.146.19.22',1245,'/home/scidb/data'
{7} 7,'member','2016-12-08 22:36:00',true,0,7,'10.146.19.22',1246,'/home/scidb/data'
{8} 8,'member','2016-12-08 22:36:00',true,0,8,'10.146.19.22',1247,'/home/scidb/data'
{9} 9,'member','2016-12-08 22:36:00',true,0,9,'10.146.19.22',1248,'/home/scidb/data'
{10} 10,'member','2016-12-08 22:36:00',true,0,10,'10.146.19.22',1249,'/home/scidb/data'
{11} 11,'member','2016-12-08 22:36:00',true,0,11,'10.146.19.22',1250,'/home/scidb/data'
{12} 12,'member','2016-12-08 22:36:00',true,0,12,'10.146.19.22',1251,'/home/scidb/data'
{13} 13,'member','2016-12-08 22:36:00',true,0,13,'10.146.19.22',1252,'/home/scidb/data'
{14} 14,'member','2016-12-08 22:36:00',true,0,14,'10.146.19.22',1253,'/home/scidb/data'
{15} 15,'member','2016-12-08 22:36:00',true,0,15,'10.146.19.22',1254,'/home/scidb/data'
{16} 4294967312,'registered',null,false,1,0,'10.139.99.10',1239,'/home/scidb/data'
{17} 4294967313,'registered',null,false,1,1,'10.139.99.10',1240,'/home/scidb/data'
{18} 4294967314,'registered',null,false,1,2,'10.139.99.10',1241,'/home/scidb/data'
{19} 4294967315,'registered',null,false,1,3,'10.139.99.10',1242,'/home/scidb/data'
{20} 4294967316,'registered',null,false,1,4,'10.139.99.10',1243,'/home/scidb/data'
{21} 4294967317,'registered',null,false,1,5,'10.139.99.10',1244,'/home/scidb/data'
{22} 4294967318,'registered',null,false,1,6,'10.139.99.10',1245,'/home/scidb/data'
{23} 4294967319,'registered',null,false,1,7,'10.139.99.10',1246,'/home/scidb/data'
{24} 4294967320,'registered',null,false,1,8,'10.139.99.10',1247,'/home/scidb/data'
{25} 4294967321,'registered',null,false,1,9,'10.139.99.10',1248,'/home/scidb/data'
{26} 4294967322,'registered',null,false,1,10,'10.139.99.10',1249,'/home/scidb/data'
{27} 4294967323,'registered',null,false,1,11,'10.139.99.10',1250,'/home/scidb/data'
{28} 4294967324,'registered',null,false,1,12,'10.139.99.10',1251,'/home/scidb/data'
{29} 4294967325,'registered',null,false,1,13,'10.139.99.10',1252,'/home/scidb/data'
{30} 4294967326,'registered',null,false,1,14,'10.139.99.10',1253,'/home/scidb/data'
{31} 4294967327,'registered',null,false,1,15,'10.139.99.10',1254,'/home/scidb/data'
```
This step may complain if the postgres port isn't opened properly or there is an issue with the data directories.

### 2.3.2 Launch the new instances
We start the actual new instance processes like so:
```bash
$ scidb.py -m p4_system start_server -si 1 mydb new_config.ini
```
Note this step uses the `new_config.ini` we created in the previous step. The instances may encounter a hiccup on starting up as they are still not fully registered with the cluster. You can verify that the actual SciDB processes are now running on the second node.

### 2.3.3 Add the new instances to the cluster
Finally we need to run this query for SciDB to add the newly launched processes to the `cluster membership`. We supply all the instance ids as shown in `list_instances()` above:
```bash
$ iquery -aq "add_instances(4294967312, 4294967313, 4294967314, 4294967315, 4294967316, 4294967317, 4294967318, 4294967319, 4294967320, 4294967321, 4294967322, 4294967323, 4294967324, 4294967325, 4294967326, 4294967327)"
```
In case you are curious - the more significant bits are used to store the Server ID - which gives these large integers.

This step may take a minute or so for the instances to "say hello" and recognize each other. You can run this query:
```bash
$ iquery -aq "sync()"
``` 
in a loop and wait for it to return success. That is how you know the cluster is operational. You can also corroborate with `list_instances()` to confirm that all 32 are now in a `member` state.

## 2.4 Run the computation on the expanded cluster
We'll create a `temp` array `test_array_shuffle`. Note that now any array we create will have the 32-instance residency:
```bash
$ iquery -aq "create temp array test_array_shuffle <val:double> [i=1:600000000:0:1000000]"
Query was executed successfully

$ iquery -anq "store(test_array, test_array_shuffle)"
Query was executed successfully
```
Now `test_array_shuffle` is stored on all 32 instances and we can see our `grouped_aggregate` takes less time to compute:
```bash
$ time iquery -otsv -aq "grouped_aggregate(test_array_shuffle, count(*), val)"
4       29989190
0       30000877
10      30009388
6       30001146
17      30002570
15      29993610
1       29995601
8       30006619
7       29997761
11      29996032
5       30001637
13      30002346
16      29994724
18      30000196
19      30000504
12      29998716
3       29993538
14      30000911
2       30006660
9       30007974

real    0m6.872s
user    0m0.009s
sys     0m0.004s
```
Quite elastic, in this case. Once again - this grouped aggregate is just an example, consider a more intensive computation that might take hours. 

Did we need to create the temp array copy? Not at all. But we do need to ensure all instances participate in the work and the array starts out only on the first 16. Another way to solve the problem is to explicitly `redistribute` with the data with the `_sg` or "scatter-gather" operator the start of the query:
```
iquery -aq "grouped_aggregate( _sg(test_array_shuffle, 1),.. )"
```
Where the `1` argument to `_sg` means "redistribute-by-hash". 

## 2.5 Save the result
Knowing the second node won't be around forever, we can save the result with a residency on the first node only. This is where our new operator actually comes in:
```
$ iquery -aq "create_with_residency(agg_result, <val:double, count:uint64> [instance_id, value_no], false, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15)"

$ iquery -anq "store(grouped_aggregate(test_array_shuffle, count(*), val), agg_result)"
Query was executed successfully
```

## 2.6 Detach and remove the second node
We perform the section 2.3 somewhat in reverse. First, remove any temp arrays
```
$ iquery -aq "remove(test_array_shuffle)"
```
Then, kill the instance processes:
```
$ scidb.py -m p4_system stop_server -si 1 mydb new_config.ini
```
Now it may again take 1-2 minutes for the cluster to recognize that the processes went unavailable. After the node's departure is recognized you will be able to successfully run this:
```
iquery -aq "remove_instances(4294967312, 4294967313, 4294967314, 4294967315, 4294967316, 4294967317, 4294967318, 4294967319, 4294967320, 4294967321, 4294967322, 4294967323, 4294967324, 4294967325, 4294967326, 4294967327)"
```
Finally, if you don't plan on re-launching this specific node in the future, you can de-register the instances from Postgres. Unfortunately there is a known issue with this step. You need to manually edit `scidb.py` and replace all occurrences of `_pgHost` with `pgHost`. Then this command will finalize the removal:
```
scidb.py -m p4_system config_server --remove config-add-delta.ini --output new_config2.ini mydb new_config.ini
```

## 2.7 Keep track of those config files
Note that in step 2.3 we created a new config.ini when we added a server and in 2.6 we created yet another config.ini file without the server again. If you plan to change the cluster state for the long term, it is recommended you register SciDB as a service with these config files:
```
$ scidb.py -m p4_system service_unregister --all mydb <old_config.ini>
$ scidb.py -m p4_system service_register --all mydb <new_config.ini> 
```
This has the effect of copying the supplied config.ini file to all nodes under `/opt/scidb/VERSION/service` and using linux tools to automatically restart _that_ config when the OS restarts. For more notes on this, see: https://paradigm4.atlassian.net/wiki/display/SD/Managing+SciDB+Instances

## In Conclusion

CE users can use this prototype to store arrays to specific locations in their cluster. EE users can grow and shrink their clusters in response to changes in demand. More fine-grained controls over array distributions schemes, such as special small arrays that are copied to every instance, and so on, are quite possible future extensions.

