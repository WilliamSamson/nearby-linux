load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "sharing_rpc_client",
    hdrs = ["sharing_rpc_client.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "grpc_async_client_factory",
    hdrs = ["grpc_async_client_factory.h"],
    deps = [":sharing_rpc_client"],
    visibility = ["//visibility:public"],
)
