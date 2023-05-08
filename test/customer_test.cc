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

    for (int i = 0; i < 2; ++i) {
        std::string pubkey = "12345" + std::to_string(i);
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

  std::pair<int, std::string> reply = clients[0]->Get(key);
  ASSERT_EQ(reply.first, 0);
  ASSERT_EQ(reply.second, value);
}

TEST_F(FCKVClientTest, SeveralPutGetOneClientTest) {
  std::string k1 = "key1";
  std::string v1 = "value1";
  std::string k2 = "key2";
  std::string v2 = "value2";
  std::string k3 = "key3";
  std::string v3 = "value3";

  ASSERT_EQ(clients[0]->Put(k1, v1), 0);
  ASSERT_EQ(clients[0]->Put(k2, v2), 0);
  ASSERT_EQ(clients[0]->Put(k3, v3), 0);

  std::pair<int, std::string> reply = clients[0]->Get(k1);
  ASSERT_EQ(reply.first, 0);
  ASSERT_EQ(reply.second, v1);
  reply = clients[0]->Get(k2);
  ASSERT_EQ(reply.first, 0);
  ASSERT_EQ(reply.second, v2);
  reply = clients[0]->Get(k3);
  ASSERT_EQ(reply.first, 0);
  ASSERT_EQ(reply.second, v3);
}

TEST_F(FCKVClientTest, PutGetMultipleClientsTest) {
  // Each of 10 clients puts key value pair
  for (int i = 0; i < 2; ++i) {
    std::string key = "100" + std::to_string(i);
    std::string value = "20" + std::to_string(i);
    ASSERT_EQ(clients[i]->Put(key, value), 0);
  }

  // Make sure each of 10 clients can get each of 10 pairs created by all clients together
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      std::string key = "100" + std::to_string(j);
      std::string value = "20" + std::to_string(j);
      std::pair<int, std::string> reply = clients[i]->Get(key);
      ASSERT_EQ(reply.first, 0);
      ASSERT_EQ(reply.second, value);
    }
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
