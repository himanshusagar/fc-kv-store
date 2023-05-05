#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>
#include <vector>
#include <string>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using fc_kv_store::FCKVStoreRPC;
using fc_kv_store::VersionStruct;

class FCKVClient
{
public:
  FCKVClient(std::shared_ptr<Channel> channel, std::string pubkey)
    : stub_(FCKVStoreRPC::NewStub(channel)),
      pubkey_(pubkey) {}
  
  std::string Get(std::string key);
  int Put(std::string key, std::string value);
  
private:
  std::unique_ptr<FCKVStoreRPC::Stub> stub_;  // gRPC
  VersionStruct version_;                     // local version struct
  std::vector<VersionStruct> versions_;       // global version structs
  std::string pubkey_;                        // this is us
  
  // acquires global lock on server
  // updates local copies of version lists as returned by the server
  //               do we need to rollback if we abort this operation??
  Status StartOp(std::vector<VersionStruct>* versions);
  
  // releases global lock on server
  // sends updated version struct to server
  Status CommitOp(VersionStruct version);
    
  // aborts the operation - releases the lock, do not send version struct
  Status AbortOp();
  
  // If a list of version structs is not totally ordered by >=, there is a
  // history conflict
  bool CheckCompatability(std::vector<VersionStruct> versions);
};