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

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  int FCKVStoreGet(int in)
  {
    // Data we are sending to the server.
    GetRequest req;
    req.set_key(in);

    // Container for the data we expect from the server.
    GetResponse reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->FCKVStoreGet(&context, req, &reply);

    // Act upon its status.
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
  FCKVStoreRPCClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  int user(96);

  auto reply = greeter.FCKVStoreGet(96);
  std::cout << "BasicRPC Int received: " << reply << std::endl;

  return 0;
}
