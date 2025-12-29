"""Module extension for libGPUCounters (Bzlmod)."""

# Repository rule for libGPUCounters
def _libgpu_counters_repo_impl(repository_ctx):
    """Repository implementation for libGPUCounters."""
    # Fetch libGPUCounters from GitHub
    # Using download_and_extract is the standard way in repository rules
    repository_ctx.download_and_extract(
        url = "https://github.com/ARM-software/libGPUCounters/archive/refs/heads/main.tar.gz",
        sha256 = "0ace698637005b48b7fab93b6e8fd41593729a2b6b455313c36bea7f0b28affc",
        stripPrefix = "libGPUCounters-main",
    )
    
    # Create REPO.bazel so Bazel recognizes this as a repository (Bzlmod)
    # REPO.bazel is just a marker file - it can be empty or contain a comment
    repository_ctx.file("REPO.bazel", "# Repository marker for libGPUCounters\n")
    
    # Create BUILD.bazel at root that re-exports libraries
    repository_ctx.file("BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")

# Re-export device library
alias(
    name = "device",
    actual = "//backend/device:device",
    visibility = ["//visibility:public"],
)

# Re-export hwcpipe library
alias(
    name = "hwcpipe",
    actual = "//hwcpipe:hwcpipe",
    visibility = ["//visibility:public"],
)
""")
    
    # Create backend/device/BUILD.bazel
    # Note: Paths in BUILD files are relative to the BUILD file's directory
    repository_ctx.file("backend/device/BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "device",
    srcs = [
        "src/device/handle.cpp",
        "src/device/hwcnt/backend_type.cpp",
        "src/device/hwcnt/reader.cpp",
        "src/device/hwcnt/sampler/detail/backend.cpp",
        "src/device/hwcnt/sampler/kinstr_prfcnt/block_index_remap.cpp",
        "src/device/hwcnt/sampler/kinstr_prfcnt/enum_info_parser.cpp",
        "src/device/hwcnt/sampler/kinstr_prfcnt/metadata_parser.cpp",
        "src/device/instance.cpp",
        "src/device/num_exec_engines.cpp",
        "src/device/product_id.cpp",
    ],
    hdrs = glob([
        "include/**/*.hpp",
        "src/**/*.hpp",
    ]),
    includes = [
        "include",
        "src",
    ],
    copts = [
        "-std=c++14",
        "-DHWCPIPE_DEVICE_BUILDING=1",
        "-Wno-unknown-warning-option",
    ],
    linkopts = ["-pthread", "-ldl"],
    visibility = ["//visibility:public"],
)
""")
    
    # Create hwcpipe/BUILD.bazel
    # Note: Paths in BUILD files are relative to the BUILD file's directory
    # The hwcpipe directory should exist after symlinking, so we can create BUILD.bazel there
    repository_ctx.file("hwcpipe/BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "hwcpipe",
    srcs = [
        "src/error.cpp",
        "src/hwcpipe/detail/counter_database.cpp",
        "src/hwcpipe/all_gpu_counters.cpp",
        "src/hwcpipe/counter_metadata.cpp",
        "src/hwcpipe/derived_functions.cpp",
    ],
    hdrs = glob([
        "include/**/*.hpp",
        "include/**/*.h",
        "src/**/*.hpp",
    ]),
    deps = ["//backend/device:device"],
    includes = [
        "include",
        "src",
    ],
    copts = [
        "-std=c++14",
        "-Wno-unknown-warning-option",
        "-Wno-switch-default",
        "-Wno-switch-enum",
    ],
    visibility = ["//visibility:public"],
)
""")

libgpu_counters_repo = repository_rule(
    implementation = _libgpu_counters_repo_impl,
)

# Module extension implementation
def _libgpu_counters_impl(module_ctx):
    """Implementation of libGPUCounters module extension."""
    # Create the repository for each module using this extension
    # In Bzlmod, we call the repository rule directly
    libgpu_counters_repo(name = "libgpu_counters")

libgpu_counters = module_extension(
    implementation = _libgpu_counters_impl,
)
