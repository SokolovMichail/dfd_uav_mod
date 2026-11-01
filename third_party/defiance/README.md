# DefianceLoader plugin ABI

`defiance.h` is the C header of the DefianceLoader plugin API, copied unchanged from
https://github.com/DefianceLoader/DefianceLoader, tag `v1.9.0`, `crates/api/include/defiance.h`.
It is under the MIT licence (`LICENSE-MIT`, from the same tag); DefianceLoader lets plugins
build against it under MIT or Apache 2.0.

Only `plugins/spares_supply/logic_dll/defiance.cpp` includes it. To update, copy the header from a
newer tag and check `DEFIANCE_ABI_VERSION` and the `DefianceApi` layout.
