#include "fc_kv_store.grpc.pb.h"
#include <string>
#include <vector>
#include "customer.h"

using fc_kv_store::GetRequest;
using fc_kv_store::GetResponse;
using fc_kv_store::PutRequest;
using fc_kv_store::PutResponse;
using fc_kv_store::StartOpRequest;
using fc_kv_store::StartOpResponse;
using fc_kv_store::CommitOpRequest;
using fc_kv_store::CommitOpResponse;

std::string FCKVClient::Get(std::string key) {
  std::vector<VersionStruct> all_versions;
  StartOp(&all_versions);

  // update local version
  // increment version number
  VersionStruct new_version = version_;
  new_version.set_version(version_.version() + 1);

  // update version vector
  for (auto v : all_versions) {
    (*new_version.mutable_vlist())[v.pubkey()] = v.version();
  }
  // don't forget about ourselves
  (*new_version.mutable_vlist())[pubkey_] = new_version.version();
  all_versions.push_back(new_version);

  // if there's a history conflict, we abort now
  if (!CheckCompatability(all_versions)) {
    std::cout << "History incompatability, aborting operation";
    AbortOp();
  }

  // Do GetRequest with H(value) from key table
  GetRequest req;
  std::string hashvalue = (*new_version.mutable_itable())[key];
  req.set_key(hashvalue);
  
  GetResponse reply;
  ClientContext context;
  Status status = stub_->FCKVStoreGet(&context, req, &reply);
  
  if (status.ok() && CommitOp(new_version).ok()) {
    version_ = new_version;
    return reply.value();
  }
  
  std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
  return nullptr;
}

int FCKVClient::Put(std::string key, std::string value) {
  PutRequest req;
  req.set_key(key);
  req.set_value(value);
    
  PutResponse reply;
  ClientContext context;
  Status status = stub_->FCKVStorePut(&context, req, &reply);

  if (status.ok())
  {
    return reply.status();
  }
  else
  {
    std::cout << status.error_code() << ": " << status.error_message()
              << std::endl;
    return -1;
  }
}

// we should expect the server to return the complete list of version structs
// TODO optimization - only return the diff between what we and the server know
Status FCKVClient::StartOp(std::vector<VersionStruct>* versions) {
  StartOpRequest req;
  StartOpResponse reply;
  ClientContext context;
  Status status = stub_->FCKVStoreStartOp(&context, req, &reply);
  if (status.ok()) {
    // update local version lists
    for (VersionStruct v : req->versions()) {
      versions->push_back(v);
    }
  }
  return status;
}

Status FCKVClient::CommitOp(VersionStruct version) {
  CommitOpRequest req;
  CommitOpResponse reply;
  ClientContext context;

  req.set_version(version);
  return stub_->FCKVStoreCommitOp(&context, req, &reply);
}
private:
  std::unique_ptr<FCKVStoreRPC::Stub> stub_;
  std::unique_ptr<VersionStruct> version_;
  std::vector<std::unique_ptr<VersionStruct>> versions_;
};

Status FCKVClient::AbortOp() {
  // return Status::ok();
}

bool FCKVClient::CheckCompatability(std::vector<VersionStruct> versions) {
  return True;
}

void sigintHandler(int sig_num)
{
  std::cerr << "Clean Shutdown\n";
  //    if (srv_ptr) {
  //        delete srv_ptr;
  //    }
  fflush(stdout);
  std::exit(0);
}

int main()
{
  // "ctrl-C handler"
  signal(SIGINT, sigintHandler);

  const std::string target_str = "localhost:50051";
  FCKVClient rpcClient(
    grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()),
    "12345");
  int user(96);
  
  std::string placedValue = "20";
  rpcClient.Put("100" , placedValue);
  auto reply = rpcClient.Get("100");
  std::cout << "BasicRPC Int received: " << reply << " " << placedValue << std::endl;

  return 0;
}
