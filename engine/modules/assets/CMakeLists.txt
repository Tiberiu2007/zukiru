# assets — Layer 1 (services). Asset manager: an importer registry, load/loadAsync
# by virtual path, ref-counted typed handles, hot-reload (re-import in place +
# version bump) and ref-counted eviction. filesystem + jobs are public deps: they
# appear in the AssetManager constructor signature (forward-declared, so headers
# stay light — but a caller must supply a FileSystem and, optionally, a JobSystem).
# Namespace zukiru::assets.
add_zukiru_module(assets PUBLIC_DEPS core filesystem jobs)
