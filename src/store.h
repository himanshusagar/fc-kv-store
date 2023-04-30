#include <iostream>
#include <sstream>
#include <string>

#include <leveldb/db.h>

#include <memory>

using std::string;

class KVStore
{
private:

    std::unique_ptr<leveldb::DB> mDB_ptr = nullptr;

public:
    KVStore(const string dbFileName);
    
    bool put(const string &key, const string& value);
    bool get(const string &key, string& value);

    bool putInt(const int key, const int value);
    bool getInt(const int key, int& value);
};