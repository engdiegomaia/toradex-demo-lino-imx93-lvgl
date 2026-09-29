# Contributing

Thanks for considering a contribution. This repository combines application
code, generated UI output, hardware configuration, and third-party components.

## Before opening a change

- Keep changes focused and describe the target hardware and software versions
  used for validation.
- Do not include device credentials, private keys, local hostnames, diagnostic
  bundles, or build output.
- Preserve upstream notices and license files when changing vendored code.
- Do not claim support for a firmware or hardware path without validating it on
  the target or clearly marking the result as unverified.

## Generated UI

GUI Guider owns most files under `app/generated/`. When changing generated UI,
include the corresponding GUI Guider project changes and note the GUI Guider
version. Required integration edits are documented in [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md);
review those before regenerating the UI.

## Firmware and device tree

Document firmware provenance and build inputs. Keep firmware patches applicable
to their stated upstream revision. Device-tree changes should identify the
module, carrier, display, and Torizon OS version they target.

## Licensing

Do not add material unless its redistribution terms are known and compatible
with the intended distribution. Preserve its license and attribution notices.
