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

std::string privateKeyFile = "private_key.pem";
std::string publicKeyFile = "public_key.pem";

bool signVersionStruct(VersionStruct* versionStruct) {
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
    *inprogress = version_;
    *tblhash = 0;
    return Status::OK;
  }

  // find our version and the max version num
  int loc = -1;
  int maxversion = -1;
  for (int i = 0; i < all_versions.size(); i++) {
    if (all_versions[i].version() > maxversion) {
      maxversion = all_versions[i].version();
    }
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
  if (maxversion != -1) {
    *tblhash = all_versions[maxversion].itablehash();
  } else {
    *tblhash = 0;
  }
  
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
  if (!signVersionStruct(inprogress)) {
    std::cerr << "Failed to sign the VersionStruct content." << std::endl;
    return Status(grpc::StatusCode::UNKNOWN, "failed to sign versionstruct");
  }

  return Status::OK;
}

std::string FCKVClient::Get(std::string key) {
  VersionStruct inprogress;
  size_t tblhash;
  if (!PreOpValidate(&inprogress, &tblhash).ok()) {
    std::cerr << "Pre-operation validation failed" << std::endl;
    exit(1);
  }

  if (!UpdateItable(tblhash).ok()) {
    std::cerr << "Problem updating keytable" << std::endl;
  }
  inprogress.set_itablehash(tblhash);
    
  // Do GetRequest with H(value) from key table
  GetRequest req;
  size_t hashvalue = (*itable_.mutable_table())[hasher_(key)];
  req.set_pubkey(pubkey_);
  req.set_key(hashvalue);
  
  GetResponse reply;
  ClientContext context;
  Status status = stub_->FCKVStoreGet(&context, req, &reply);
  
  if (status.ok() && CommitOp(inprogress).ok()) {
    // TODO does this copy actually work?
    version_ = inprogress;
    return reply.value();
  }
  
  std::cout << status.error_code() << ": " << status.error_message()
            << std::endl;
  return nullptr;
}

int FCKVClient::Put(std::string key, std::string value) {
  VersionStruct inprogress;
  size_t tblhash;
  if (!PreOpValidate(&inprogress, &tblhash).ok()) {
    std::cerr << "Pre-operation validation failed" << std::endl;
  }

  if (!UpdateItable(tblhash).ok()) {
    std::cerr << "Problem updating keytable" << std::endl;
  }
  inprogress.set_itablehash(tblhash);
    
  // Do PutRequest
  PutRequest req;
  PutResponse reply;
  ClientContext context;
  req.set_pubkey(pubkey_);
  req.set_value(value);
  Status status = stub_->FCKVStorePut(&context, req, &reply);

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
  tblreq.set_pubkey(pubkey_);
  tblreq.set_value(serializeditable);
  status = stub_->FCKVStorePut(&tblcontext, tblreq, &tblreply);

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
    return 0;
  }
  
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

    req.set_pubkey(pubkey_);
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
  req.set_pubkey(pubkey_);
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
  *msgversion = version;
  req.set_pubkey(pubkey_);
  return stub_->FCKVStoreCommitOp(&context, req, &reply);
}

// use pubkey to abort an operation and unlock the server
// this is not a secure solution
Status FCKVClient::AbortOp() {
  AbortOpRequest req;
  AbortOpResponse reply;
  ClientContext context;

  req.set_pubkey(pubkey_);
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
