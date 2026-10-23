## What and why

<!-- The problem, and why this change is the right fix. Link the milestone or decision (docs/decisions.md). -->

## Evidence

<!-- Measurements with their setup (data, hardware, build type), or the tests that fail without this change. Numbers are measured, never estimated. -->

## Checklist

- [ ] Tests cover the change and fail without it
- [ ] `ctest --preset release` and `ctest --preset asan` pass
- [ ] Public API documented; `doxygen docs/Doxyfile` has no warnings
- [ ] `docs/architecture.md` and the README match the new behaviour
- [ ] Decisions with lasting consequences recorded in `docs/decisions.md`
- [ ] Limitations and anything not verified stated below

## Not verified

<!-- What this PR could not test, and why. -->
