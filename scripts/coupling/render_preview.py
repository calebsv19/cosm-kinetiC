"""Bounded local renders of exported accepted states; no renderer source mutation."""
import hashlib
import json
from pathlib import Path
import subprocess
import time
from surface_sources.growth_fire_v1 import require, sealed
from cfd_evidence import admitted_path


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def render(export_root, renderer, revisions):
    root = admitted_path(export_root); worker = admitted_path(renderer)
    manifest = json.loads((root / 'playback.json').read_text())
    require(manifest == sealed(manifest), 'playback manifest integrity')
    require(type(revisions) is list and 1 <= len(revisions) <= 4 and len(set(revisions)) == len(revisions), 'one to four distinct preview revisions')
    rows = {row['revision']: row for row in manifest['frames']}
    require(all(type(v) is int and v in rows for v in revisions), 'accepted preview revision required')
    original = sha(worker); results = []; started = time.monotonic()
    for revision in revisions:
        row = rows[revision]; request = admitted_path(root / row['request']); directory = request.parent
        require(request.is_relative_to(root), 'preview input containment')
        inputs = {path: sha(path) for path in directory.iterdir() if path.is_file()}
        require(sha(request) == row['request_sha256'] and sha(directory / 'playback_scene_runtime.json') == row['scene_sha256']
                and sha(directory / 'frame_000000.vf3d') == row['vf3d_sha256'], 'preview input identity')
        for mode in ('preflight', 'render'):
            require(sha(worker) == original, 'renderer bytes changed')
            summary = directory / (mode + '-summary.json')
            require(not summary.exists(), 'choose a fresh export before rendering')
            command = [str(worker), '--request', str(request), '--' + mode, '--summary', str(summary), '--summary-file-only']
            execution = subprocess.run(command, capture_output=True, text=True, timeout=180)
            (directory / (mode + '.stderr.log')).write_text(execution.stderr)
            require(execution.returncode == 0, mode + ' failed: ' + execution.stderr[-4000:])
        result = json.loads((directory / 'render-summary.json').read_text())
        require(result['scene_applied'] and result['frames_rendered'] == 1, 'rendered scene/frame proof')
        require(result['diagnostics'] == 'ok' and result['volume_attached'] and result['volume_visible']
                and result['volume_summary']['has_data'] and result['volume_summary']['layout_valid'],
                'accepted native smoke must be attached and valid in the preview')
        image = directory / 'preview/frames/frame_0000.bmp'
        require(image.is_file() and image.stat().st_size > 54, 'preview image missing')
        require(all(sha(path) == before for path, before in inputs.items()), 'render changed accepted input')
        results.append({'revision': revision, 'time_s': row['time_s'], 'checkpoint_digest': row['checkpoint_digest'],
                        'image': str(image.relative_to(root)), 'image_sha256': sha(image),
                        'summary_sha256': sha(directory / 'render-summary.json')})
    require(sha(worker) == original, 'renderer bytes changed')
    receipt = sealed({'schema': 'fire_fluid_local_preview/v1', 'renderer_sha256': original,
                      'playback_digest': manifest['digest'], 'elapsed_s': time.monotonic() - started,
                      'frames': results, 'physical_calibration_qualified': False, 'scene_promotion': False})
    (root / ('preview-receipt-' + '-'.join('%03d' % v for v in revisions) + '.json')).write_text(json.dumps(receipt, indent=2) + '\n')
    return receipt
