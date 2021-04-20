# libraft
![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)


## Overview
libraft is a C++ raft library,based on the [RAFT](https://raft.github.io/) consistency algorithm,inspired by [etcd/raft](https://github.com/etcd-io/etcd/tree/master/raft).

## Features
- Leader election and priority-based semi-deterministic leader election
- Cluster membership management, adding nodes, removing nodes, replacing nodes, etc.
- Mechanism of transfer leader for reboot, load balance scene, etc.
- Symmetric network partition tolerance
- Asymmetric network partition tolerance
- Fault tolerance, minority failure doesn't affect the overall availability of system
- Manual recovery cluster available for majority failure
- Linearizable read, ReadIndex/LeaseRead
- Replication pipeline

## Dependencies
- CMake: >=2.6
- Protobuf: >=1.8.0
- gtest: >=3.14.0

## Build

```
# first build dependencies(if system include these dependencies,just ignore it)
./bootstap.sh

# second build libraft
mkdir build
cd build
cmake ..
make

# run tests
./bin/libraft_test
```

## Documents


## Contribution


## Acknowledgement
libraft was ported from Etcd's raft module[etcd/raft](https://github.com/etcd-io/etcd/tree/master/raft) with some optimizing and improvement. Thanks to the etcd team for opening up such a great RAFT implementation.

## License
libraft is licensed under the [Apache License 2.0](./LICENSE). 