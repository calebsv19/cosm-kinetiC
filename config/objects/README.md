# Shape Assets (`config/objects/`)

Canonical, flattened geometry assets shared across programs. Each file uses the ShapeAsset JSON schema:

```json
{
  "schema": 1,
  "name": "example_shape",
  "paths": [
    { "closed": true, "points": [ { "x": 0, "y": 0 }, { "x": 1, "y": 0 } ] }
  ]
}
```

Use `shape_asset_tool` to convert legacy ShapeLib JSON from `import/` into this format:

```
make shape_asset_tool
./shape_asset_tool --max-error 0.5 --out config/objects/airfoil_basic.asset.json import/airfoil.json
```

At runtime the physics sim loads every `*.asset.json` in this directory into the ShapeAsset library. The editor’s “Add from JSON” picker prefers these assets; if you select a legacy `import/*.json`, the editor will convert it to `config/objects/<name>.asset.json`, add it to the preset, and resolve it against the loaded library.
