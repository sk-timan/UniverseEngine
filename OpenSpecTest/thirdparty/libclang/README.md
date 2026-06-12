# Vendored libclang (build-time only)

`ReflectionGenerator` links this package. `OpenSpecTest` does **not** link libclang.

## Layout

```
libclang/
  VERSION
  include/          # clang-c/Index.h, etc.
  lib/
    Debug/          # libclang.lib
    Release/
  bin/
    Debug/          # libclang.dll
    Release/
```

## Upgrade

1. Replace `include/`, `lib/`, and `bin/` from the same LLVM release.
2. Update `VERSION`.
3. Rebuild `ReflectionGenerator` and run reflection codegen.

No system LLVM install is required when this directory is populated.
