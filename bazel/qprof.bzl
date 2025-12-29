"""Module extension for QProf library (Bzlmod)."""

# Repository rule for QProf
def _qprof_repo_impl(repository_ctx):
    """Repository implementation for QProf."""
    # Get QProf home from environment
    qprof_home = repository_ctx.os.environ.get("QPROF_HOME", "/opt/qcom/Shared/QualcommProfiler/API")
    
    include_path = qprof_home + "/include"
    lib_path = qprof_home + "/target-la/aarch64/libs"
    
    # Create symlinks to the QProf directories so we can use relative paths in glob
    # Try to create symlinks - if they fail, we'll handle it gracefully
    include_symlink_created = False
    
    # Check if the include path exists
    include_path_obj = repository_ctx.path(include_path)
    if include_path_obj.exists:
        # Create symlink to include directory
        repository_ctx.symlink(include_path, "include")
        include_symlink_created = True
    
    # Generate BUILD file with relative paths if symlink was created, otherwise use absolute paths in includes
    if include_symlink_created:
        repository_ctx.file("BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "qprof",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    linkopts = [
        "-L{lib_path}",
        "-lQualcommProfilerApi",
    ],
    visibility = ["//visibility:public"],
)
""".format(lib_path = lib_path))
    else:
        # If symlink failed, we need to use a different approach
        # Create a filegroup that references the absolute path
        # Note: We can't use glob with absolute paths, so we'll list the headers explicitly
        # or use a different method
        repository_ctx.file("BUILD.bazel", """
load("@rules_cc//cc:defs.bzl", "cc_library")

# Note: If symlink creation failed, you may need to set QPROF_HOME correctly
# or manually create symlinks. For now, we'll create an empty library that
# requires manual configuration.
cc_library(
    name = "qprof",
    hdrs = [],
    includes = ["{include_path}"],
    linkopts = [
        "-L{lib_path}",
        "-lQualcommProfilerApi",
    ],
    visibility = ["//visibility:public"],
)
""".format(include_path = include_path, lib_path = lib_path))

qprof_repo = repository_rule(
    implementation = _qprof_repo_impl,
    environ = ["QPROF_HOME"],
    local = True,
)

# Module extension implementation
def _qprof_impl(module_ctx):
    """Implementation of qprof module extension."""
    # Create the repository for each module using this extension
    # In Bzlmod, we call the repository rule directly
    qprof_repo(name = "qprof")

qprof = module_extension(
    implementation = _qprof_impl,
)
