# Private namespace probe

This probe is adapted from the `SHORiN-KiWATA/we-proton` test-source commit
`87a32e9864242185e3ec8d213c71bf9117cd053d`; the 0013 implementation originated
in commit `2d2af782bc096469b8c44918529de35230bb6ec`.
The source is kept as an API-conformance test, with the original attribution and
reference outputs preserved. It is not a game-specific test.

The probe checks boundary-descriptor layout, all exported namespace entry points,
duplicate and invalid aliases, case-insensitive aliases, named mutex/event/
semaphore/timer/section/job objects, cross-process opening, and close/destroy
behavior. `reference/` contains the Windows and WE-Proton Wine outputs supplied by
the source project. The remaining differences documented there are expected and
must not be silently treated as a regression.

Build the 64-bit and 32-bit probes with `build.sh`; run them only in disposable
prefixes with `run.sh`, then apply the stable API assertions with `verify.sh`.
Never run the probe against the Games bottle or a live game.

