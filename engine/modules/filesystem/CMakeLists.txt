# filesystem — Layer 1 (services). Virtual file system: mount points, virtual
# path resolution, read/write. Public API depends on core; the implementation
# uses platform (file I/O primitives). Namespace zuki::filesystem.
add_zuki_module(filesystem
  PUBLIC_DEPS  core
  PRIVATE_DEPS platform)
