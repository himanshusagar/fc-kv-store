#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>
#include <vector>
#include <string>
#include <filesystem>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using fc_kv_store::FCKVStoreRPC;
using fc_kv_store::VersionStruct;
using fc_kv_store::KeyTable;

class FCKVClient
{
public:
  FCKVClient(std::shared_ptr<Channel> channel, std::string pubkey, const std::string& privateKeyFile, const std::string& publicKeyFile)
    : stub_(FCKVStoreRPC::NewStub(channel)),
      pubkey_(pubkey),
      privateKeyFile_(privateKeyFile),
      publicKeyFile_(publicKeyFile) {
        if (std::filesystem::exists(privateKeyFile) || std::filesystem::exists(publicKeyFile)) {
            // std::cout << "Log: Private or public key file already exists. Overwriting the files." << std::endl;
        }

        if (generateRSAKeyPair(privateKeyFile, publicKeyFile)) {
            // std::cout << "Log: RSA key pair generated successfully." << std::endl;
        } else {
            throw std::runtime_error("Failed to generate RSA key pair.");
        }
      }
  
  std::pair<int, std::string> Get(std::string key);
  int Put(std::string key, std::string value);
  
private:
  std::unique_ptr<FCKVStoreRPC::Stub> stub_;  // gRPC
  VersionStruct version_;                     // local version struct
  KeyTable itable_;
  std::vector<VersionStruct> versions_;       // global version structs
  std::string pubkey_;                        // this is us
  std::hash<std::string> hasher_;

  std::string privateKeyFile_;
  std::string publicKeyFile_;

  // Call PreOpValidate before any call to Get or Put
  // This function start the operation with the server and checks for
  // invalid or conflicting information from the server.
  Status PreOpValidate(VersionStruct* inprogress, size_t* tblhash);
  
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

  bool generateRSAKeyPair(std::string privateKeyFile, std::string publicKeyFile);

  // fetch key table from most recent version if it is different than ours
  Status UpdateItable(size_t tablehash);
};

void sigintHandler(int sig_num);