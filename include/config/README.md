# `include/config/`

Headers for configuration loaders and related utilities.

- `config_loader.h` – declares the loader that fills `AppConfig` structures from JSON files (defaults today, parsing soon). Include this from `main.c` or any tooling that needs to inspect config files.
