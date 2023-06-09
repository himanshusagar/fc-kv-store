#include "fc_kv_store.grpc.pb.h"

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
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
using fc_kv_store::AbortOpRequest;
using fc_kv_store::AbortOpResponse;
using fc_kv_store::UserVersion;

using fc_kv_store::TamperInfoRequest;
using fc_kv_store::TamperInfoResponse;


// std::string privateKeyFile = "private_key.pem";
// std::string publicKeyFile = "public_key.pem";

bool signVersionStruct(VersionStruct* versionStruct, std::string privateKeyFile) {
    // Serialize the VersionStruct without the signature field
    versionStruct->clear_signature();
    std::string serializedData = versionStruct->SerializeAsString();

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
    versionStruct->set_signature(signature.data(), signature.size());

    return true;
}

Status FCKVClient::PreOpValidate(VersionStruct* inprogress, size_t* tblhash) {
  std::vector<VersionStruct> all_versions;
  StartOp(&all_versions);

  if (all_versions.size() == 0) {
    std::cout << "all versions was 0 sized" << std::endl;
    inprogress->CopyFrom(version_);
    *tblhash = 0;
    return Status::OK;
  }

  // find our version and the max version num
  int maxversionloc = 0;
  for (int i = 1; i < all_versions.size(); i++) {
    if (all_versions[i].version() > all_versions[maxversionloc].version()) {
      maxversionloc = i;
    }
  }
  
  int loc = -1;
  for (int i = 0; i < all_versions.size(); i++) {
    if (all_versions[i].pubkey() == pubkey_) {
      // just abort if the signatures don't match, there is a problem here
      if (all_versions[i].signature() != version_.signature()) {
        std::cout << "Bad version!" << std::endl;
        return Status(grpc::StatusCode::UNKNOWN, "signature mismatch");
      }
      loc = i;
    }
  }
  
  // server doesn't have a version for us yet, so add ours to the list
  if (loc == -1) {
    all_versions.push_back(version_);
    loc = all_versions.size() - 1;
  }

  // remember the most recent itable hash
  *tblhash = all_versions[maxversionloc].itablehash();
  int maxversion = all_versions[maxversionloc].version();
  
  // validate and increment version number
  all_versions[loc].set_version(maxversion + 1);

  // if there's a history conflict, we abort now
  // from now on, our version is the last one in the all_versions list
  // because it is sorted by version() in CheckCompatability
  if (!CheckCompatability(all_versions)) {
    std::cout << "History incompatability, aborting operation" << std::endl;
    AbortOp();
    return Status(grpc::StatusCode::UNKNOWN, "history incompatability");
  }
  *inprogress = all_versions[all_versions.size() - 1];

  // update version vector
  for (auto v : all_versions) {
    UserVersion* uv = inprogress->add_vlist();
    uv->set_user(hasher_(v.pubkey()));
    uv->set_version(v.version());
  }
  
  // sign new version struct
  if (!signVersionStruct(inprogress, privateKeyFile_)) {
    std::cerr << "Failed to sign the VersionStruct content." << std::endl;
    return Status(grpc::StatusCode::UNKNOWN, "failed to sign versionstruct");
  }

  return Status::OK;
}

std::pair<int, std::string> FCKVClient::Get(std::string key) {
  VersionStruct inprogress;
  size_t tblhash;
  if (!PreOpValidate(&inprogress, &tblhash).ok()) {
    std::cerr << "Pre-operation validation failed" << std::endl;
    return std::make_pair(-1, "Error occurred");
  }

  grpc::Status update_status = UpdateItable(tblhash);
  if (!update_status.ok()) {
    std::cout << "Problem updating keytable in get - UpdateItable error: " << update_status.error_code() << ": " << update_status.error_message() << std::endl;
    return std::make_pair(-1, "Error occurred");
  }
  inprogress.set_itablehash(tblhash);
    
  // Do GetRequest with H(value) from key table
  GetRequest req;
  size_t hashvalue = (*itable_.mutable_table())[hasher_(key)];
  req.set_pubkey(hasher_(pubkey_));
  req.set_key(hashvalue);
  
  GetResponse reply;
  ClientContext context;
  Status status = stub_->FCKVStoreGet(&context, req, &reply);
  
  if (status.ok() && CommitOp(inprogress).ok()) {
    // TODO does this copy actually work?
    std::cout << "Log: Successfully completed get"
              << std::endl;
    version_.CopyFrom(inprogress);
    return std::make_pair(0, reply.value());
  }
  
  std::cout << "Log: Failed to complete get"
              << std::endl;
  std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
  return std::make_pair(-1, "Error occurred");
}

int FCKVClient::TamperInfo(std::string key, std::string value, int type)
{
  TamperInfoRequest req;
  TamperInfoResponse reply;
  ClientContext context;
  req.set_key(hasher_(pubkey_));
  req.set_value(value);

  if(type == 1)
  {
    req.set_tampertype(1); // ServerTamperInfoHideUpdate
  }
  else if(type == 2)
  {
    req.set_tampertype(2); // ServerTamperInfoHideUpdate
  }
  Status status = stub_->FCKVServerTamperInfo(&context, req, &reply);
  if(status.ok())
    return 0;
  return 1;
}

int FCKVClient::Put(std::string key, std::string value) {
  VersionStruct inprogress;
  size_t tblhash;
  if (!PreOpValidate(&inprogress, &tblhash).ok()) {
    std::cerr << "Pre-operation validation failed" << std::endl;
    return -1;
  }
  grpc::Status update_status = UpdateItable(tblhash);
  if (!update_status.ok()) {
    std::cout << "Problem updating keytable in put - UpdateItable error: " << update_status.error_code() << ": " << update_status.error_message() << std::endl;
    return -1;
  }
  inprogress.set_itablehash(tblhash);
    
  // Do PutRequest
  PutRequest req;
  PutResponse reply;
  ClientContext context;
  req.set_pubkey(hasher_(pubkey_));
  req.set_value(value);
  Status status = stub_->FCKVStorePut(&context, req, &reply);
  
  if (!status.ok()) {
    std::cout << "Error on line 196: " << status.error_code() << ": " << status.error_message() << std::endl;
  }

  size_t hashvalue = hasher_(value);
  size_t serverhash = reply.hash();

  if (hashvalue != serverhash) {
    std::cout << "Server hashed our value differently than we did. Error!"
              << std::endl;
    return -1;
  }

  // update key table
  (*itable_.mutable_table())[hasher_(key)] = hashvalue;
  std::string serializeditable;
  itable_.SerializeToString(&serializeditable);
  
  PutRequest tblreq;
  PutResponse tblreply;
  ClientContext tblcontext;
  tblreq.set_pubkey(hasher_(pubkey_));
  tblreq.set_value(serializeditable);
  status = stub_->FCKVStorePut(&tblcontext, tblreq, &tblreply);

  if (!status.ok()) {
    std::cout << "Error on line 221: " << status.error_code() << ": " << status.error_message() << std::endl;
  }

  hashvalue = hasher_(serializeditable);
  serverhash = tblreply.hash();

  if (hashvalue != serverhash) {
    std::cout << "Server hashed our itable differently than we did. Error!"
              << std::endl;
    return -1;
  }

  inprogress.set_itablehash(hashvalue);
  
  if (status.ok() && CommitOp(inprogress).ok()) {
    // TODO does this copy actually work?
    version_ = inprogress;
    std::cout << "Log: Successfully completed put"
              << std::endl;
    return 0;
  }
  
  std::cout << "Log: Failed to complete put"
              << std::endl;
  std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
  return -1;
}

// fetch key table from most recent version if it is different than ours
Status FCKVClient::UpdateItable(size_t latest_itablehash) {
  Status status = Status::OK;
  if (latest_itablehash == 0) {
    return status;
  }

  std::string local_itable;
  itable_.SerializeToString(&local_itable);

  if (latest_itablehash != hasher_(local_itable)) {
    GetRequest req;
    GetResponse reply;
    ClientContext context;

    req.set_pubkey(hasher_(pubkey_));
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
  req.set_pubkey(hasher_(pubkey_));
  Status status = stub_->FCKVStoreStartOp(&context, req, &reply);
  if (status.ok()) {
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

  VersionStruct* msgversion = req.mutable_v();
  msgversion->CopyFrom(version);
  req.set_pubkey(hasher_(pubkey_));
  return stub_->FCKVStoreCommitOp(&context, req, &reply);
}

// use pubkey to abort an operation and unlock the server
// this is not a secure solution
Status FCKVClient::AbortOp() {
  AbortOpRequest req;
  AbortOpResponse reply;
  ClientContext context;

  req.set_pubkey(hasher_(pubkey_));
  return stub_->FCKVStoreAbortOp(&context, req, &reply);
}

bool FCKVClient::CheckCompatability(std::vector<VersionStruct> versions) {
  try {
  std::sort(versions.begin(), versions.end(),
            [](const VersionStruct& a, const VersionStruct& b) {
              if (a.version() == b.version()) throw std::logic_error("error");
              return a.version() < b.version();
            });
  }
  catch (std::exception& e) {
    return false;
  }
  
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

bool FCKVClient::generateRSAKeyPair(std::string privateKeyFile, std::string publicKeyFile) {
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

// int main()
// {
//   // "ctrl-C handler"
//   signal(SIGINT, sigintHandler);

//   // --START-- Generate private and public key
//   if (std::filesystem::exists(privateKeyFile) || std::filesystem::exists(publicKeyFile)) {
//       std::cerr << "Private or public key file already exists. Please remove or rename them before generating a new key pair." << std::endl;
//       return 1;
//   }

//   if (generateRSAKeyPair()) {
//       std::cout << "RSA key pair generated successfully." << std::endl;
//   } else {
//       std::cerr << "Failed to generate RSA key pair." << std::endl;
//       return 1;
//   }
//   // --END-- Generate private and public key

//   const std::string target_str = "localhost:50051";
//   FCKVClient rpcClient(
//     grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()),
//     "12345");
//   int user(96);
  
//   std::string placedValue = "20";
//   rpcClient.Put("100" , placedValue);
//   auto reply = rpcClient.Get("100");
//   std::cout << "BasicRPC Int received: " << reply << " " << placedValue << std::endl;

//   return 0;
// }
