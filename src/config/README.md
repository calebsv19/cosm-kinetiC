# `src/config/`

Runtime helpers that ingest disk configs and populate `AppConfig`.

- `config_loader.c` – current stub that seeds defaults and confirms the JSON file is reachable. Once a JSON parser is chosen, this file will apply values from `config/app.json` into the `AppConfig`.

Anything that transforms user-editable config files into runtime structs should live here so `main.c` can stay a thin bootstrapper.
