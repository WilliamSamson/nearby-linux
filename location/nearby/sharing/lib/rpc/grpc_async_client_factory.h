// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_
#define LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_

#include <memory>
#include "location/nearby/sharing/lib/rpc/sharing_rpc_client.h"

namespace nearby::sharing::platform::common {

class GrpcAsyncClientFactory {
 public:
  GrpcAsyncClientFactory(void* account_manager, void* clock, void* analytics_recorder) {}
  virtual ~GrpcAsyncClientFactory() = default;

  virtual std::unique_ptr<nearby::sharing::api::SharingRpcClient> CreateInstance() { return nullptr; }
  virtual std::unique_ptr<nearby::sharing::api::IdentityRpcClient> CreateIdentityInstance() { return nullptr; }
};

}  // namespace nearby::sharing::platform::common

#endif  // LOCATION_NEARBY_SHARING_LIB_RPC_GRPC_ASYNC_CLIENT_FACTORY_H_
