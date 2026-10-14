#!/usr/bin/env python3
"""Render one to four accepted playback states locally, with a per-command 180 s cap."""
import argparse
import json
from pathlib import Path
from coupling.render_preview import render

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--export', dest='export_root', type=Path, required=True)
parser.add_argument('--renderer', type=Path, required=True)
parser.add_argument('--revisions', required=True, help='comma-separated accepted revisions, maximum four')
args = parser.parse_args()
print(json.dumps(render(args.export_root, args.renderer, [int(v) for v in args.revisions.split(',')]), indent=2))
