# Contributing to Fiasco

Thanks for taking the time to contribute. This document covers the workflow,
the build, and the conventions the project follows.

## Developer Certificate of Origin

Every commit must carry a `Signed-off-by` trailer. That is the
[Developer Certificate of Origin](DCO) (DCO 1.1): you certify that you wrote the
change, or that you have the right to submit it under the project's license.

Sign off with:

```shell
git commit -s -m "feat(scope): describe the change"
```

The trailer is added automatically. If you forget it, amend the commit:

```shell
git commit --amend -s --no-edit
```

## Build

Fiasco requires **CMake ≥ 3.30** and **GCC ≥ 16** (C++26 reflection). The
supported compiler is GCC for now.

```shell
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Optional flags:

```shell
cmake -S . -B build -DFIASCO_BUILD_TESTS=OFF
cmake -S . -B build -DFIASCO_BUILD_EXAMPLES=OFF
```

C++20 modules are parked for now; the sources remain under `modules/`, but
there is no module build target.

Sanitizer builds are useful for anything touching the server or the request
lifecycle:

```shell
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build-asan
ctest --test-dir build-asan
```

Use `thread` instead of `address` for TSan.

## Commits

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
feat(scope): add a feature
fix(scope): fix a bug
docs(scope): change documentation
refactor(scope): change code without changing behavior
test(scope): add or change tests
ci: change the CI configuration
chore: change tooling or maintenance
```

Keep the subject line imperative and under 72 characters. Use the body for
*why* the change is needed, not *what* the diff does.

## Branches

Work on a branch, not on `main`. Name it by the change:

- `feat/<topic>` for new features
- `fix/<topic>` for fixes
- `ci/<topic>` for CI
- `chore/<topic>` for maintenance
- `docs/<topic>` for documentation

Open a pull request against `main`. Keep the change focused; a PR that does two
unrelated things is two PRs.

## Tests

- Every behavior change needs a test. Prefer a test that fails before the change
  and passes after it.
- The suite is Catch2. Run `ctest --test-dir build` before opening the PR.
- Tests that bind a port use port `0` and the in-process test client; do not
  hard-code a port.
- Add a test to the matching `tests/CMakeLists.txt` target, not to a new target
  unless the behavior is genuinely new.

## Formatting

- Format with `.clang-format` (`clang-format -i`).
- The codebase is C++26. Use `std::` types, `[[nodiscard]]` where it applies,
  and the existing naming conventions.
- No macros in the public API. Reflection is the mechanism.

## License

By contributing, you agree that your contributions are licensed under the MIT
license in [LICENSE](LICENSE).
