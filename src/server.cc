#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>
#include "server.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

Status FCKVStoreRPCServiceImpl::FCKVStoreStartOp(
  ServerContext* context, const StartOpRequest* req, StartOpResponse* res) {
  if (lock_.empty()) {
    lock_ = req->pubkey();
  
    for (auto& [pubkey, vs] : vsl_) {
      res->add_versions(vs);
    }
    return Status::OK;
  } else {
    return Status(grpc::StatusCode::UNAVAILABLE, "");
  }
}
  
Status FCKVStoreRPCServiceImpl::FCKVStoreCommitOp(
  ServerContext* context, const CommitOpRequest* req, CommitOpResponse* res) {
  if (lock_ == req->pubkey()) {
    std::string version;
    req->v().SerializeToString(&version);
    vsl_[hasher_(req->pubkey())] = version;
    return Status::OK;
  } else {
    return Status(grpc::StatusCode::UNAVAILABLE, "");
  }
}

Status FCKVStoreRPCServiceImpl::FCKVStoreAbortOp(
  ServerContext* context, const AbortOpRequest* req, AbortOpResponse* res) {
  if (lock_ == req->pubkey()) {
    lock_.clear();
    return Status::OK;
  } else {
    return Status(grpc::StatusCode::UNAVAILABLE, "");
  }
}
    
Status FCKVStoreRPCServiceImpl::FCKVStoreGet(
  ServerContext* context, const GetRequest* request, GetResponse* reply) {
  if (lock_ == request->pubkey()) {
    leveldb::Status status;
    std::string value;
    std::string key = std::to_string(request->key());
    status = store_->Get(leveldb::ReadOptions(), key.c_str(), &value);
    if (status.ok()) {
      reply->set_value(value);
      return Status::OK;
    }
    std::cout << "Server Get error with key " << key << std::endl;
    return Status(grpc::StatusCode::NOT_FOUND, "");
  } else {
    return Status(grpc::StatusCode::UNAVAILABLE, "");
  }
}

Status FCKVStoreRPCServiceImpl::FCKVStorePut(
  ServerContext* context, const PutRequest* request, PutResponse* reply) {
  if (lock_ == request->pubkey()) {
    leveldb::Status status;
    std::string val = request->value();
    size_t hashval = hasher_(val);
    status = store_->Put(leveldb::WriteOptions(), std::to_string(hashval), val);
    if (status.ok()) {
      std::cout << "Store Put OK" << std::endl;
      reply->set_hash(hashval);
      return Status::OK;
    }
    std::cout << "Server Put error with key " << hashval << std::endl;
    return Status(grpc::StatusCode::UNKNOWN, "");
  } else {
    return Status(grpc::StatusCode::UNAVAILABLE, "");
  }
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

void run_server()
{
  std::string server_address("localhost:50051");
  FCKVStoreRPCServiceImpl service;
  //  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);

  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main()
{
  // "ctrl-C handler"
  signal(SIGINT, sigintHandler);
  run_server();
  return 0;
}
