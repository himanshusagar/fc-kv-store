# FC KV Store

## Installation Steps
1. sudo apt update
2. sudo apt-get install libleveldb-dev
3. mkdir -p ~/dev/install/
4. cd ~/dev/install/
5. git clone https://github.com/Microsoft/vcpkg.git && cd vcpkg
6. bash bootstrap-vcpkg.sh
7. ./vcpkg install leveldb
8. cd ~/dev
9. git clone git@github.com:himanshusagar/fc-kv-store.git
10. mkdir build && cd build && cmake ../ && make
11. cd bin
12. ./simple_kv_store
