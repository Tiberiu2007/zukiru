# jobs — Layer 1 (services). Thread-pool job system: fire-and-forget tasks,
# futures, and a blocking parallel-for. Public API depends only on core; the
# implementation uses platform (hardwareConcurrency, thread naming).
# Namespace zukiru::jobs.
add_zukiru_module(jobs
  PUBLIC_DEPS  core
  PRIVATE_DEPS platform)
