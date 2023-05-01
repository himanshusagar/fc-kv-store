#include "hash_table.h"

using fc_kv_store::OuterTableKey;
using fc_kv_store::OuterTableValue;

using fc_kv_store::InnerTableKey;
using fc_kv_store::InnerTableValue;

using std::cout;
using std::endl;


bool HashTable::putOuter(const int key, const int value, string& valueHash)
{
    // Generate Key
    OuterTableKey keyEntry;
    keyEntry.set_key(key);
    string keyString;
    keyEntry.SerializeToString(&keyString);
    

    // Generte Key+Value
    OuterTableValue valueEntry;
    valueEntry.set_key(key);
    valueEntry.set_value(value);
    string valueString;
    valueEntry.SerializeToString(&valueString);
    valueHash = std::to_string( std::hash<string>{}(valueString) );

    return mKVStore->put(keyString , valueHash);
}

bool HashTable::putInner(const string& key, const int value)
{
    // Generte Key
    InnerTableKey keyEntry;
    keyEntry.set_key(key);
    string keyString;
    keyEntry.SerializeToString(&keyString);
    

    // Generte Value
    InnerTableValue valueEntry;
    valueEntry.set_value(value);
    string valueString;
    valueEntry.SerializeToString(&valueString);

    return mKVStore->put(keyString , valueString);

}
bool HashTable::put(const int key, const int value)
{
    string valueHash;
    if(!putOuter(key, value, valueHash))
    {
        return false;
    }

    if(!putInner(valueHash, value))
    {
        return false;
    }
    
    return true;
}


bool HashTable::getInner(const string& key, int& value)
{

    // Generte Key
    InnerTableKey keyEntry;
    keyEntry.set_key(key);
    string keyString;
    keyEntry.SerializeToString(&keyString);


    string valueString;
    if(! mKVStore->get(keyString , valueString) )
        return false;

    // Get Value
    InnerTableValue valueEntry;
    valueEntry.ParseFromString(valueString);
    value = valueEntry.value();
    return true; 

}
bool HashTable::getOuter(const int key, string& hashedValueOuter)
{
    // Generate Key
    OuterTableKey keyEntry;
    keyEntry.set_key(key);
    string keyString;
    keyEntry.SerializeToString(&keyString);

    return mKVStore->get(keyString , hashedValueOuter);
}

 bool HashTable::get(const int key, int& value)
 {
    string hashedValueOuter;
    if(!getOuter(key, hashedValueOuter))
    {
        return false;
    }

    int innerValue; 
    if(!getInner(hashedValueOuter, innerValue))
    {
        return false;
    }
      
    
    // Generate key+value for comparison
    OuterTableValue valueEntry;
    valueEntry.set_key(key);
    valueEntry.set_value(innerValue);
    string valueString;
    valueEntry.SerializeToString(&valueString);
    string valueHash = std::to_string( std::hash<string>{}(valueString) );

    
    if(hashedValueOuter == valueHash)
    {
        //Verification Successful
        value = innerValue;
        return true;
    }
    else
    {
        cout << "verification failed for" << key << " " << innerValue << endl; 
    }

    return false;

    
 }