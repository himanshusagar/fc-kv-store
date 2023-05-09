#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <leveldb/db.h>
#include <atomic>
#include <thread>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using fc_kv_store::FCKVStoreRPC;
using fc_kv_store::GetRequest;
using fc_kv_store::GetResponse;
using fc_kv_store::PutRequest;
using fc_kv_store::PutResponse;
using fc_kv_store::StartOpRequest;
using fc_kv_store::StartOpResponse;
using fc_kv_store::CommitOpRequest;
using fc_kv_store::CommitOpResponse;
using fc_kv_store::AbortOpRequest;
using fc_kv_store::AbortOpResponse;
using fc_kv_store::VersionStruct;

using fc_kv_store::TamperInfoRequest;
using fc_kv_store::TamperInfoResponse;


class FCKVStoreRPCServiceImpl final : public FCKVStoreRPC::Service
{
public:
  FCKVStoreRPCServiceImpl()
    : lock_(0) {
    leveldb::Options options;
    options.create_if_missing = true;
    std::string dbPath = "/tmp/kv_store";
    leveldb::Status status = leveldb::DB::Open(options, dbPath,  &store_);
    if (!status.ok()) {
      std::cout << "Error opening leveldb with path " << dbPath << std::endl;
    }
  }

  Status FCKVStoreStartOp(ServerContext* context, const StartOpRequest* req,
                          StartOpResponse* res) override;

  Status FCKVStoreCommitOp(ServerContext* context, const CommitOpRequest* req,
                           CommitOpResponse* res) override;

  Status FCKVStoreAbortOp(ServerContext* context, const AbortOpRequest* req,
                          AbortOpResponse* res) override;

  Status FCKVStoreGet(ServerContext* context, const GetRequest* request,
                      GetResponse* reply) override;
  
  Status FCKVStorePut(ServerContext* context, const PutRequest* request,
                      PutResponse* reply) override;

  Status FCKVServerTamperInfo(ServerContext* context, const TamperInfoRequest* request,
                      TamperInfoResponse* reply) override;


  enum ServerTamperInfo
  {
    ServerTamperInfoNone = 0,
    ServerTamperInfoHideUpdate = 1,
    ServerTamperInfoBadData = 2
  };

private:
  leveldb::DB* store_;
  std::map<size_t, std::string> vsl_; // hash(pubkey) -> VersionStruct as str
  std::hash<std::string> hasher_;
  std::atomic<size_t> lock_;
  ServerTamperInfo tamper_info_;
  
};
