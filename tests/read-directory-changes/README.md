# ReadDirectoryChangesW probe

This probe exercises generic non-recursive `ReadDirectoryChangesW` semantics against a temporary
directory. It covers create, modify, delete, rename, and no-change cancellation. Rename accepts
either the preferred old/new pair or the documented removed+added fallback.

Build both 64-bit and 32-bit PE probes with `build.sh`. Run them only in disposable prefixes with
`run.sh`; validate output with `verify.sh`. The Wine 11 engine is wow64-only, so the 32-bit probe
uses a separate win64 prefix in the same way as the private-namespace probe. Pass the optional
fourth argument (`read_directory_changes32.exe`) to run the 32-bit PE probe.
