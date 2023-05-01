#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using fc_kv_store::FCKVStoreRPC;
using fc_kv_store::GetRequest;
using fc_kv_store::GetResponse;
using fc_kv_store::PutRequest;
using fc_kv_store::PutResponse;

class FCKVStoreRPCClient
{
public:
  FCKVStoreRPCClient(std::shared_ptr<Channel> channel)
      : stub_(FCKVStoreRPC::NewStub(channel)) {}

  int FCKVStoreGet(int in)
  {
    GetRequest req;
    req.set_key(in);

    GetResponse reply;
    ClientContext context;
    Status status = stub_->FCKVStoreGet(&context, req, &reply);

    if (status.ok())
    {
      return reply.value();
    }
    else
    {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return -1;
    }
  }

  int FCKVStorePut(int key, int value)
  {
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

private:
  std::unique_ptr<FCKVStoreRPC::Stub> stub_;
};

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
  FCKVStoreRPCClient rpcClient(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  int user(96);

  int placedValue = 20;
  rpcClient.FCKVStorePut(100 , placedValue);
  auto reply = rpcClient.FCKVStoreGet(100);
  std::cout << "BasicRPC Int received: " << reply << " " << placedValue << std::endl;

  return 0;
}
