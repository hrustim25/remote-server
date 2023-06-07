# Remote server

Basic version of remote execution server. Now supports only "spawn" command, creating connection and running 1 command.

## Setup

To setup run

```
mkdir build
cd build
cmake ..
make
```

## Usage

To launch server use
```
./server <port>
```

To launch client use
```
./client <server address:port> spawn <command> [args]
```
