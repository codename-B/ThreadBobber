# Yarn Spinner bindings

`yarn_spinner.proto` is an unmodified copy from Yarn Spinner **v3.2.2**:
https://github.com/YarnSpinnerTool/YarnSpinner/blob/v3.2.2/YarnSpinner/yarn_spinner.proto

The upstream MIT license is included as `LICENSE.YarnSpinner.md`.
The generated `../yarn_spinner_pb2.py` is checked in so converter users need only
the `protobuf` runtime, not a protobuf compiler.

To regenerate from the repository root:

```sh
python -m pip install -r requirements-dev.txt
python -m grpc_tools.protoc -I proto --python_out=. proto/yarn_spinner.proto
```

To update Yarn Spinner support, replace the schema from the chosen upstream tag,
update this provenance and the compiler pin, regenerate, and review instruction
and operand changes against the converter and C runtime. Generating bindings does
not implement new runtime behaviour. Run `make generate`, `make test`, and
`make parity` and review fixture changes before accepting an update.

The converter sorts nodes by their protobuf map key because protobuf map order
is unspecified. This may change generated IDs and program hashes compared with
the former wire-order decoder; rebuild generated C and use matching save data.
