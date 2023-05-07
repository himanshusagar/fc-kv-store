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
    client = std::make_unique<FCKVClient>(channel, "12345");
  }

  std::unique_ptr<FCKVClient> client;
};

TEST_F(FCKVClientTest, PutGetTest) {
  std::string key = "100";
  std::string value = "20";
  ASSERT_EQ(client->Put(key, value), 0);

  std::string reply = client->Get(key);
  ASSERT_EQ(reply, value);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}