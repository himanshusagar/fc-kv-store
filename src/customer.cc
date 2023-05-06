#include "fc_kv_store.grpc.pb.h"

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "customer.h"

using fc_kv_store::GetRequest;
using fc_kv_store::GetResponse;
using fc_kv_store::PutRequest;
using fc_kv_store::PutResponse;
using fc_kv_store::StartOpRequest;
using fc_kv_store::StartOpResponse;
using fc_kv_store::CommitOpRequest;
using fc_kv_store::CommitOpResponse;

std::string privateKeyFile = "private_key.pem";
std::string publicKeyFile = "public_key.pem";

bool signVersionStruct(VersionStruct& versionStruct) {
    // Serialize the VersionStruct without the signature field
    versionStruct.clear_signature();
    std::string serializedData = versionStruct.SerializeAsString();

    // Load the private key from the file
    FILE* privateKeyFilePtr = fopen(privateKeyFile.c_str(), "rb");
    if (!privateKeyFilePtr) {
        std::cerr << "Failed to open private key file." << std::endl;
        return false;
    }

    RSA* privateKey = PEM_read_RSAPrivateKey(privateKeyFilePtr, nullptr, nullptr, nullptr);
    fclose(privateKeyFilePtr);
    if (!privateKey) {
        std::cerr << "Failed to read private key from file." << std::endl;
        return false;
    }

    // Calculate the hash of the serialized data
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, serializedData.c_str(), serializedData.size());
    SHA256_Final(hash, &sha256);

    // Sign the hash using the private key
    unsigned int signatureLen = RSA_size(privateKey);
    std::vector<unsigned char> signature(signatureLen);
    int result = RSA_sign(NID_sha256, hash, SHA256_DIGEST_LENGTH, signature.data(), &signatureLen, privateKey);
    RSA_free(privateKey);

    if (result != 1) {
        std::cerr << "Failed to sign the VersionStruct content." << std::endl;
        return false;
    }

    // Set the signature field in the VersionStruct
    versionStruct.set_signature(signature.data(), signature.size());

    return true;
}

std::string FCKVClient::Get(std::string key) {
  std::vector<VersionStruct> all_versions;
  StartOp(&all_versions);

  // update local version
  // increment version number
  VersionStruct new_version = version_;
  new_version.set_version(version_.version() + 1);

  // update version vector
  for (auto v : all_versions) {
    (*new_version.mutable_vlist())[hasher_(v.pubkey())] = v.version();
  }
  // don't forget about ourselves
  (*new_version.mutable_vlist())[hasher_(pubkey_)] = new_version.version();
  all_versions.push_back(new_version);

  // sign new version struct
  if (!signVersionStruct(new_version)) {
    std::cerr << "Failed to sign the VersionStruct content." << std::endl;
  }

  // if there's a history conflict, we abort now
  if (!CheckCompatability(all_versions)) {
    std::cout << "History incompatability, aborting operation" << std::endl;
    AbortOp();
  }

  if (!UpdateItable(all_versions[0].itablehash()).ok()) {
    std::cerr << "Problem updating keytable" << std::endl;
  }
    
  // Do GetRequest with H(value) from key table
  GetRequest req;
  size_t hashvalue = (*itable_.mutable_table())[hasher_(key)];
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
//   PutRequest req;
//   req.set_key(key);
//   req.set_value(value);
    
//   PutResponse reply;
//   ClientContext context;
//   Status status = stub_->FCKVStorePut(&context, req, &reply);

//   if (status.ok())
//   {
//     return reply.status();
//   }
//   else
//   {
//     std::cout << status.error_code() << ": " << status.error_message()
//               << std::endl;
//     return -1;
//   }
  return 0;
}

// fetch key table from most recent version if it is different than ours
Status FCKVClient::UpdateItable(size_t latest_itablehash) {
  std::string local_itable;
  itable_.SerializeToString(&local_itable);

  Status status = Status::OK;
  if (latest_itablehash != hasher_(local_itable)) {
    GetRequest req;
    GetResponse reply;
    ClientContext context;

    req.set_key(latest_itablehash);
    status = stub_->FCKVStoreGet(&context, req, &reply);
    // TODO validate (check hashes match)
    if (status.ok()) {
      itable_.ParseFromString(reply.value());
    }
  }
  return status;
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
    for (std::string v : reply.versions()) {
      VersionStruct version;
      version.ParseFromString(v);
      versions->push_back(version);
    }
  }
  return status;
}

Status FCKVClient::CommitOp(VersionStruct version) {
  CommitOpRequest req;
  CommitOpResponse reply;
  ClientContext context;

  req.set_allocated_v(&version);
  return stub_->FCKVStoreCommitOp(&context, req, &reply);
}

Status FCKVClient::AbortOp() {
  // return Status::ok();
}

bool FCKVClient::CheckCompatability(std::vector<VersionStruct> versions) {
  return true;
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

bool generateRSAKeyPair() {
    int keyLength = 2048;
    int exp = RSA_F4; // Standard RSA exponent: 65537

    // Generate RSA key
    RSA* rsa = RSA_generate_key(keyLength, exp, nullptr, nullptr);
    if (!rsa) {
        std::cerr << "Failed to generate RSA key pair." << std::endl;
        return false;
    }

    // Write private key to file
    FILE* privateKeyFilePtr = fopen(privateKeyFile.c_str(), "wb");
    if (!privateKeyFilePtr) {
        std::cerr << "Failed to open private key file." << std::endl;
        RSA_free(rsa);
        return false;
    }

    if (!PEM_write_RSAPrivateKey(privateKeyFilePtr, rsa, nullptr, nullptr, 0, nullptr, nullptr)) {
        std::cerr << "Failed to write private key to file." << std::endl;
        fclose(privateKeyFilePtr);
        RSA_free(rsa);
        return false;
    }

    fclose(privateKeyFilePtr);

    // Write public key to file
    FILE* publicKeyFilePtr = fopen(publicKeyFile.c_str(), "wb");
    if (!publicKeyFilePtr) {
        std::cerr << "Failed to open public key file." << std::endl;
        RSA_free(rsa);
        return false;
    }

    if (!PEM_write_RSAPublicKey(publicKeyFilePtr, rsa)) {
        std::cerr << "Failed to write public key to file." << std::endl;
        fclose(publicKeyFilePtr);
        RSA_free(rsa);
        return false;
    }

    fclose(publicKeyFilePtr);
    RSA_free(rsa);

    return true;
}

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
