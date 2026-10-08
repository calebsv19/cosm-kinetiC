"""Copy-on-transform release app stages; execution requires owning release authority.

This source helper does not grant signing, notarization or publication authority.
Its transaction preserves the input package and all failed staged attempts.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

from contract_proof import execute
from desktop_replace import check_bundle, inventory
from package_outputs import declare, plan
from package_transaction import run, parse_assignments
from release_notary import accepted_binding

BINARIES = ('physics-sim-bin', 'physics_sim_session_worker', 'physics-sim-launcher')


def admit(source, bundle_id):
    rows = check_bundle(source, bundle_id)
    for name in BINARIES:
        relative = 'Contents/MacOS/' + name
        if rows.get(relative, {}).get('kind') != 'file':
            raise ValueError('Release stage requires regular binary: ' + name)
    if rows.get('Contents/Frameworks', {}).get('kind') != 'directory':
        raise ValueError('Release stage requires Frameworks directory')
    return rows


def assemble(repo, source, bundle_id, phase, tool, identity, assignments, notary_receipt=None):
    expected = {'RELEASE_ROOT', 'RELEASE_DIR', 'APP_DEST', 'APP_REPORTS'}
    if set(assignments) != expected:
        raise ValueError('Release stage requires exact transaction mappings')
    root = assignments['RELEASE_ROOT']
    if assignments['RELEASE_DIR'] != root:
        raise ValueError('Release stage root mapping mismatch')
    destination = assignments['APP_DEST']; reports = assignments['APP_REPORTS']
    for output in (destination, reports):
        if (source.is_relative_to(output) or output.is_relative_to(source)
                or output == root):
            raise ValueError('Release stage output overlaps its input or root')
    if destination.is_relative_to(reports) or reports.is_relative_to(destination):
        raise ValueError('Release app and reports overlap')
    before = admit(source, bundle_id)
    acceptance=None
    if phase=='staple':
        if notary_receipt is None:raise ValueError('Stapling requires bound notarization receipt')
        acceptance=accepted_binding(repo,notary_receipt,source)
    elif notary_receipt is not None:raise ValueError('Signing cannot consume notarization evidence')
    validation = lambda: plan(repo, root, [destination, reports], [])
    declared = validation()
    declare(declared, validation)
    if sys.platform == 'darwin':
        execute(['/usr/bin/ditto', '--rsrc', '--extattr', '--acl', str(source), str(destination)],
                repo, reports, 'copy', (), 300, 1048576)
    else:
        shutil.copytree(source, destination, symlinks=True, dirs_exist_ok=True)
    if inventory(destination) != before or inventory(source) != before:
        raise ValueError('Release app changed during stage copy')
    targets = [name for name, row in sorted(before.items())
               if name.startswith('Contents/Frameworks/')
               and name.endswith('.dylib') and row['kind'] == 'file']
    targets += ['Contents/MacOS/' + name for name in BINARIES]
    if phase == 'sign':
        for index, name in enumerate(targets + ['.']):
            flags = ['--timestamp=none'] if identity == '-' else ['--timestamp']
            if identity != '-' and not name.startswith('Contents/Frameworks/'):
                flags += ['--options', 'runtime']
            execute([tool, '--force', '--sign', identity] + flags + [str(destination / name)],
                    repo, reports, 'sign_' + str(index), (), 300, 1048576)
        execute([tool, '--verify', '--deep', '--strict', str(destination)],
                repo, reports, 'verify', (), 300, 1048576)
    elif phase == 'staple':
        execute([tool, 'stapler', 'staple', str(destination)],
                repo, reports, 'staple', (), 300, 1048576)
        execute([tool, 'stapler', 'validate', str(destination)],
                repo, reports, 'validate', (), 300, 1048576)
    else:
        raise ValueError('Unsupported release app phase')
    admit(destination, bundle_id)
    if inventory(source) != before:
        raise ValueError('Release input changed during transformation')
    if acceptance is not None and accepted_binding(repo,notary_receipt,source)!=acceptance:
        raise ValueError('Notarization acceptance changed during stapling')
    with (reports / 'stage.json').open('x') as stream:
        json.dump({'phase': phase, 'bundle_id': bundle_id,
                   'source_preserved': True, 'tool_checks_completed': True, 'notary_acceptance':acceptance,
                   'release_authority_granted': False,
                   'public_or_registry_acceptance_verified': False}, stream, indent=2)
        stream.write('\n')


def stage(repo, source, root, bundle_id, phase, tool, identity='-', *, notary_receipt=None):
    source = Path(os.path.abspath(source)); root = Path(os.path.abspath(root))
    if phase not in ('sign', 'staple'):
        raise ValueError('Unsupported release app phase')
    if not identity or '\x00' in identity:
        raise ValueError('Missing or invalid signing identity')
    if source.is_relative_to(root) or root.is_relative_to(source):
        raise ValueError('Release stage root overlaps its input')
    admit(source, bundle_id)
    acceptance=None
    if phase=='staple':
        if notary_receipt is None:raise ValueError('Stapling requires bound notarization receipt')
        notary_receipt=Path(os.path.abspath(notary_receipt))
        acceptance=accepted_binding(repo,notary_receipt,source)
    elif notary_receipt is not None:raise ValueError('Signing cannot consume notarization evidence')
    helper = Path(__file__).resolve()
    command = [sys.executable, '-B', str(helper), '_assemble', '--source', str(source),
               '--bundle-id', bundle_id, '--phase', phase, '--tool', tool,
               '--identity=' + identity]
    extra_inputs=[];extra_values=[]
    if acceptance is not None:
        command+=['--notary-receipt',str(notary_receipt)]
        extra_inputs=[Path(name) for name in acceptance['inputs']]
        extra_values=['notary_acceptance_sha256='+acceptance['receipt_sha256']]
    return run(repo, root, {'APP_DEST': root / source.name,
                          'APP_REPORTS': root / 'command-reports'}, {}, {},
               [source, helper.parent]+extra_inputs, ['phase=' + phase, 'bundle_id=' + bundle_id,
                                      'signing_identity=' + identity]+extra_values, command, [tool])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('sign', 'staple', '_assemble'))
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--bundle-id', required=True)
    parser.add_argument('--tool', required=True)
    parser.add_argument('--phase', choices=('sign', 'staple'))
    parser.add_argument('--identity', default='-')
    parser.add_argument('--notary-receipt',type=Path)
    args, mappings = parser.parse_known_args()
    try:
        repo = Path.cwd().resolve(); source = Path(os.path.abspath(args.source))
        if args.action == '_assemble':
            if args.root is not None or args.phase is None:
                raise ValueError('Assembly requires phase and transaction root mappings')
            assemble(repo, source, args.bundle_id, args.phase, args.tool,
                     args.identity, parse_assignments(mappings),args.notary_receipt)
        else:
            if mappings or args.phase is not None or args.root is None:
                raise ValueError('Stage requires selected root and no assembly mappings')
            print(json.dumps(stage(repo, source, args.root, args.bundle_id,
                                   args.action, args.tool, args.identity,notary_receipt=args.notary_receipt)))
    except (ValueError, OSError) as error:
        parser.exit(2, 'Release app stage held: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
