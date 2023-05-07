#include "customer.h"

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <exception>

int main()
{
  // "ctrl-C handler"
  signal(SIGINT, sigintHandler);

  // --START-- Generate private and public key
  if (std::filesystem::exists(privateKeyFile) || std::filesystem::exists(publicKeyFile)) {
      std::cerr << "Private or public key file already exists. Please remove or rename them before generating a new key pair." << std::endl;
      return 1;
  }

  if (generateRSAKeyPair()) {
      std::cout << "RSA key pair generated successfully." << std::endl;
  } else {
      std::cerr << "Failed to generate RSA key pair." << std::endl;
      return 1;
  }
  // --END-- Generate private and public key

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