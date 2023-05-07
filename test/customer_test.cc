#include "customer.h"
#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

class FCKVClientTest : public testing::Test {
protected:
  void SetUp() override {
    std::string server_address("localhost:50051");
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());

    for (int i = 0; i < 10; ++i) {
        std::string pubkey = "client" + std::to_string(i) + "_pubkey";
        std::string privateKeyFile = "client" + std::to_string(i) + "_private_key.pem";
        std::string publicKeyFile = "client" + std::to_string(i) + "_public_key.pem";

        clients.push_back(std::make_unique<FCKVClient>(channel, pubkey, privateKeyFile, publicKeyFile));
    }
  }

  std::vector<std::unique_ptr<FCKVClient>> clients;
};

TEST_F(FCKVClientTest, PutGetOneClientTest) {
  std::string key = "100";
  std::string value = "20";
  ASSERT_EQ(clients[0]->Put(key, value), 0);

  std::string reply = clients[0]->Get(key);
  ASSERT_EQ(reply, value);
}

TEST_F(FCKVClientTest, PutGetMultipleClientsTest) {
  // Each of 10 clients puts key value pair
  for (int i = 0; i < 10; ++i) {
    std::string key = "100" + std::to_string(i);
    std::string value = "20" + std::to_string(i);
    ASSERT_EQ(clients[i]->Put(key, value), 0);
  }

  // Make sure each of 10 clients can get each of 10 pairs created by all clients together
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      std::string key = "100" + std::to_string(j);
      std::string value = "20" + std::to_string(j);
      std::string reply = clients[i]->Get(key);
      ASSERT_EQ(reply, value);
    }
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}