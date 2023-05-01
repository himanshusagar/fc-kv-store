#include "store.h"

using std::unique_ptr;

KVStore::KVStore(const string dbFileName)
{
    leveldb::DB* localPtr = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, dbFileName,  &localPtr );
    mDB_ptr = unique_ptr<leveldb::DB>(localPtr);
    assert(status.ok());
}

bool KVStore::put(const string &key, const string& value)
{
    leveldb::Status status = mDB_ptr->Put(leveldb::WriteOptions(), key, value);
    return status.ok();
}
bool KVStore::get(const string &key, string& value)
{
    leveldb::Status status = mDB_ptr->Get(leveldb::ReadOptions(), key, &value);
    return status.ok();
}

