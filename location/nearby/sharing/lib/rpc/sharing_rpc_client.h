#ifndef LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_
#define LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/timestamp.pb.h"
#include "sharing/proto/contact_rpc.pb.h"

namespace google::nearby::identity::v1 {

class SharedCredential {
 public:
  enum DataType {
    DATA_TYPE_UNSPECIFIED = 0,
    DATA_TYPE_PUBLIC_CERTIFICATE = 1,
  };

  void set_id(uint64_t id) { id_ = std::to_string(id); }
  const std::string& id() const { return id_; }

  void set_data(const std::string& data) { data_ = data; }
  const std::string& data() const { return data_; }

  void set_data_type(DataType data_type) { data_type_ = data_type; }
  DataType data_type() const { return data_type_; }

  google::protobuf::Timestamp* mutable_expiration_time() {
    return &expiration_time_;
  }

 private:
  std::string id_;
  std::string data_;
  DataType data_type_ = DATA_TYPE_UNSPECIFIED;
  google::protobuf::Timestamp expiration_time_;
};

class PerVisibilitySharedCredentials {
 public:
  enum Visibility {
    VISIBILITY_UNSPECIFIED = 0,
    VISIBILITY_SELF = 1,
    VISIBILITY_CONTACTS = 2,
  };

  void set_visibility(Visibility visibility) { visibility_ = visibility; }
  SharedCredential* add_shared_credentials() {
    shared_credentials_.emplace_back();
    return &shared_credentials_.back();
  }

 private:
  Visibility visibility_ = VISIBILITY_UNSPECIFIED;
  std::vector<SharedCredential> shared_credentials_;
};

class Device {
 public:
  enum Contact {
    CONTACT_UNSPECIFIED = 0,
    CONTACT_GOOGLE_CONTACT = 1,
    CONTACT_GOOGLE_CONTACT_LATEST = 2,
  };

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }
  void set_contact(Contact contact) { contact_ = contact; }

  PerVisibilitySharedCredentials* add_per_visibility_shared_credentials() {
    per_visibility_shared_credentials_.emplace_back();
    return &per_visibility_shared_credentials_.back();
  }

 private:
  std::string name_;
  std::string display_name_;
  Contact contact_ = CONTACT_UNSPECIFIED;
  std::vector<PerVisibilitySharedCredentials> per_visibility_shared_credentials_;
};

class QuerySharedCredentialsRequest {
 public:
  void set_name(const std::string& name) { name_ = name; }
  void set_page_token(const std::string& page_token) {
    page_token_ = page_token;
  }

 private:
  std::string name_;
  std::string page_token_;
};

class QuerySharedCredentialsWithBindingIdsRequest
    : public QuerySharedCredentialsRequest {};

class QuerySharedCredentialsResponse {
 public:
  const std::vector<SharedCredential>& shared_credentials() const {
    return shared_credentials_;
  }
  const std::string& next_page_token() const { return next_page_token_; }

 private:
  std::vector<SharedCredential> shared_credentials_;
  std::string next_page_token_;
};

class QuerySharedCredentialsWithBindingIdsResponse
    : public QuerySharedCredentialsResponse {};

class PublishDeviceRequest {
 public:
  Device* mutable_device() { return &device_; }
  const Device& device() const { return device_; }

 private:
  Device device_;
};

class PublishDeviceResponse {
 public:
  enum ContactUpdate {
    CONTACT_UPDATE_UNSPECIFIED = 0,
    CONTACT_UPDATE_REMOVED = 1,
  };

  const std::vector<ContactUpdate>& contact_updates() const {
    return contact_updates_;
  }

 private:
  std::vector<ContactUpdate> contact_updates_;
};

class GetAccountInfoRequest {};

class AccountInfo {
 public:
  enum Capability {
    CAPABILITY_UNSPECIFIED = 0,
    CAPABILITY_TITANIUM = 1,
  };

  const std::vector<Capability>& capabilities() const {
    return capabilities_;
  }

 private:
  std::vector<Capability> capabilities_;
};

class GetAccountInfoResponse {
 public:
  const AccountInfo& account_info() const { return account_info_; }

 private:
  AccountInfo account_info_;
};

}  // namespace google::nearby::identity::v1

namespace nearby::sharing::api {

class SharingRpcClient {
 public:
  virtual ~SharingRpcClient() = default;
  virtual void ListContactPeople(
      const nearby::sharing::proto::ListContactPeopleRequest& request,
      std::function<void(const nearby::sharing::proto::ListContactPeopleResponse&)> callback) {}
};

class IdentityRpcClient {
 public:
  static constexpr absl::Duration kTimeout = absl::Seconds(30);

  virtual ~IdentityRpcClient() = default;

  template <typename Callback>
  void QuerySharedCredentials(
      google::nearby::identity::v1::QuerySharedCredentialsRequest request,
      absl::Duration timeout, Callback callback) {
    (void)request;
    (void)timeout;
    callback(google::nearby::identity::v1::QuerySharedCredentialsResponse());
  }

  template <typename Callback>
  void QuerySharedCredentialsWithBindingIds(
      google::nearby::identity::v1::QuerySharedCredentialsWithBindingIdsRequest
          request,
      absl::Duration timeout, Callback callback) {
    (void)request;
    (void)timeout;
    callback(google::nearby::identity::v1::
                 QuerySharedCredentialsWithBindingIdsResponse());
  }

  template <typename Callback>
  void PublishDevice(
      google::nearby::identity::v1::PublishDeviceRequest request,
      absl::Duration timeout, Callback callback) {
    (void)request;
    (void)timeout;
    callback(google::nearby::identity::v1::PublishDeviceResponse());
  }

  template <typename Callback>
  void GetAccountInfo(
      google::nearby::identity::v1::GetAccountInfoRequest request,
      absl::Duration timeout, Callback callback) {
    (void)request;
    (void)timeout;
    callback(google::nearby::identity::v1::GetAccountInfoResponse());
  }
};

}  // namespace nearby::sharing::api

#endif  // LOCATION_NEARBY_SHARING_LIB_RPC_SHARING_RPC_CLIENT_H_
