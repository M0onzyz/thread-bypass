# thread-bypass
This is a simple proof-of-concept bypass for Hyperion's thread creation filters. The code creates a suspended thread externally using `CreateRemoteThreadEx`, whitelists it, and then resumes execution. This approach tricks Hyperion’s `LdrInitializeThunk` callback into treating the thread as if it were created internally, allowing it to run without being blocked.
