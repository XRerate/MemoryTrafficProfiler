"""Module extension for libGPUCounters (Bzlmod)."""

# Repository rule for libGPUCounters
def _libgpu_counters_repo_impl(repository_ctx):
    """Repository implementation for libGPUCounters."""
    # The module source is fetched by the registry to @@libgpu_counters+
    # We need to access it. Since we can't use Label (requires BUILD files),
    # we'll use the repository_ctx's path resolution differently.
    # Actually, we can use repository_ctx.path() with a string path
    # The module should be at a known location in the external directory
    # Try to find it by looking for the MODULE.bazel file
    # Or use the workspace root and construct the path
    
    # Get workspace root
    workspace_file = repository_ctx.path(Label("//:WORKSPACE")).dirname if hasattr(repository_ctx, "path") else None
    if not workspace_file:
        # Try alternative: the module is in external/libgpu_counters+
        # We can construct this path
        # Actually, let's use os.environ or a different method
        # The simplest: use the module extension to write the path to a file
        # that the repository rule can read
        pass
    
    # For now, let's try accessing the external directory directly
    # The path format in Bazel's external directory is predictable
    # But we need the actual external root. Let's use a workaround:
    # Create BUILD files that reference the module source via external paths
    # Or better: have the module extension create a file with the path
    
    # Actually, the simplest solution: in the module extension, use mod.source_path
    # to get the path and create the repository with that knowledge
    # But repository rules can't easily receive this. 
    # Let's try: use repository_ctx.os.environ or a file written by the extension
    
    # Workaround: The module source path is available in module_ctx
    # We'll pass it via an environment variable or file
    # For now, let's try constructing it: external/<canonical_repo_name>
    # The canonical name for libgpu_counters from registry would be libgpu_counters+
    
    # Try to access via a known pattern - the module is fetched to external
    # We can use repository_ctx.path() with a relative path if we know the structure
    # Actually, let's just try to symlink to where we know it should be
    # The module from registry goes to external/libgpu_counters+ (or similar)
    
    # Get the external root by going up from current repo
    current_repo = repository_ctx.name
    # The module repo should be @@libgpu_counters+ 
    # We can't directly reference it, but we can try to find it
    # Let's use a different approach: download it again in the repo rule
    # But that's wasteful. Better: use the fact that module_ctx has the path
    
    # Final approach: The module extension has mod.source_path
    # We can use that to create a file that the repo rule reads
    # Or, we can make the repo rule re-download (not ideal but works)
    
    # For now, let's have the repo rule re-fetch the source
    # It's not ideal but it will work
    repository_ctx.download_and_extract(
        url = "https://github.com/ARM-software/libGPUCounters/archive/aa7751b8854a8093b2efdacabec480ca4d3f29b3.tar.gz",
        sha256 = "944b4949bc3f7a22437f0ffd456a3063baf0e2dff82d63a10902cde310684d88",
        stripPrefix = "libGPUCounters-aa7751b8854a8093b2efdacabec480ca4d3f29b3",
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

def _libgpu_counters_repo_impl_v2(repository_ctx):
    """Repository implementation for libGPUCounters - uses module_ctx."""
    # This will be called from the module extension with access to module_ctx
    # For now, use a workaround: the module source is in the external cache
    # We can construct the path: external/libgpu_counters+
    # But we need the actual path. Let's try a different approach:
    # Use the module extension's path() method
    pass

libgpu_counters_repo = repository_rule(
    implementation = _libgpu_counters_repo_impl,
    attrs = {},
)

# Module extension implementation
def _libgpu_counters_impl(module_ctx):
    """Implementation of libGPUCounters module extension."""
    # Create the repository for each module using this extension
    # In Bzlmod, we call the repository rule directly
    libgpu_counters_repo(name = "libgpu_counters_build")

libgpu_counters = module_extension(
    implementation = _libgpu_counters_impl,
)
