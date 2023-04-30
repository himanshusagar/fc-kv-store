#include "fc_kv_store.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <signal.h>

#include "store.h"

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


class FCKVStoreRPCServiceImpl final : public FCKVStoreRPC::Service
{

private:
    KVStore *mKVStore = new KVStore("/tmp/kv_store.db");;

public:
    
    Status FCKVStoreGet(ServerContext* context, const GetRequest* request
                           , GetResponse* reply) override
    {   
        int value;
        mKVStore->getInt( request->key() , value );
        reply->set_key( request->key() );
        reply->set_value( value );
        return Status::OK;
    }
    Status FCKVStorePut(ServerContext* context, const PutRequest* request
                           , PutResponse* reply) override
    {
        bool status = mKVStore->putInt( request->key() , request->value() );
        reply->set_status(status);
        return Status::OK;
    }

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
}
