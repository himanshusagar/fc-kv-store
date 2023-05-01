#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <functional>

#include <leveldb/db.h>

#include "store.h"

using std::string;

class HashTable
{
private:
    KVStore *mKVStore = new KVStore("/tmp/kv_store.db");;

    bool putOuter(const int key, const int value, string& valueHash);
    bool putInner(const string& key, const int value);

    bool getOuter(const int key, string& hashedValueOuter);
    bool getInner(const string& key, int& value);

public:
    bool put(const int key, const int value);
    bool get(const int key, int& value);
    
};