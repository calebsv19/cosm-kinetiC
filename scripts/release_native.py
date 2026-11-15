"""Producer phases for the Registry native owner; no submission or retry owner.

Signing reuses release_app_stage. The Registry retains the signed checkpoint,
notary submission identity and native ticket claim. This adapter only prepares
the upload and packages an already accepted, validated app transactionally.
"""
import argparse
import json
import os
from pathlib import Path
import sys
import uuid

from check_clean_root import read_json
from contract_proof import execute
from desktop_replace import inventory
from package_outputs import plan, declare
from package_transaction import run, parse_assignments
from release_final_artifact import bundle_identity, identity_fields
from release_zip_validation import verify


def identity(version):
    selected = dict(product='kinetiC', program='physics_sim',
                    bundle_id='com.cosm.kinetic', version=version,
                    channel='stable', platform='macOS', arch='arm64')
    identity_fields(selected)
    if any(c not in '0123456789.' for c in version):
        raise ValueError('Invalid release version')
    return selected


def accepted(root):
    value = read_json(root/'notary_submit.json', 65536)
    if not isinstance(value, dict) or value.get('status') != 'Accepted':
        raise ValueError('Native owner has no Accepted notarization')
    if str(uuid.UUID(value['id'])) != value['id']:
        raise ValueError('Invalid notarization submission identity')
    return value


def assemble(repo, root, selected, phase, mappings):
    expected = {'RELEASE_ROOT', 'RELEASE_DIR', 'ARCHIVE', 'REPORTS'}
    if phase == 'package':
        expected |= {'CHECKSUM', 'MANIFEST'}
    if set(mappings) != expected or mappings['RELEASE_ROOT'] != mappings['RELEASE_DIR']:
        raise ValueError('Native archive transaction mapping mismatch')
    stage = mappings['RELEASE_ROOT']; reports = mappings['REPORTS']
    files = [mappings[key] for key in ('ARCHIVE', 'CHECKSUM', 'MANIFEST') if key in mappings]
    validation = lambda: plan(repo, stage, [reports], files)
    declare(validation(), validation)
    app = root/'kinetiC.app'
    bundle_identity(app, selected)
    before = inventory(app)
    evidence = accepted(root) if phase == 'package' else None
    def command(argv, tag):
        return execute(argv, repo, reports, tag, (), 300, 1048576)
    command(['codesign', '--verify', '--deep', '--strict', str(app)], 'signature')
    if phase == 'package':
        command(['xcrun', 'stapler', 'validate', str(app)], 'ticket')
        command(['spctl', '--assess', '--type', 'execute', '--verbose=2', str(app)], 'gatekeeper')
    archive = mappings['ARCHIVE']
    command(['/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', '--keepParent',
             str(app), str(archive)], 'archive')
    proof = verify(archive, app)
    if inventory(app) != before or (evidence is not None and accepted(root) != evidence):
        raise ValueError('Native archive input changed')
    if phase == 'package':
        with mappings['CHECKSUM'].open('x') as stream:
            stream.write(proof['archive_sha256']+'  '+archive.name+'\n')
        fields = dict(selected, format='zip', artifact=archive.name,
                      sha256=proof['archive_sha256'], signed='1', notarized='1',
                      sha256_file=mappings['CHECKSUM'].name, notary_json='notary_submit.json')
        with mappings['MANIFEST'].open('x') as stream:
            stream.write(''.join(key+'='+value+'\n' for key, value in fields.items()))
    with (reports/'verification.json').open('x') as stream:
        json.dump(dict(zip=proof, phase=phase, notary=evidence), stream)


def archive(repo, root, version, phase):
    selected = identity(version); app = root/'kinetiC.app'
    bundle_identity(app, selected)
    name = ('kinetiC-'+version+'-macOS-arm64-stable-notary-submit.zip' if phase == 'upload' else
            'kinetiC-'+version+'-macOS-arm64-stable.zip')
    files = {'ARCHIVE': root/name}
    inputs = [app, Path(__file__).resolve().parent]
    tools = ['codesign', '/usr/bin/ditto']
    if phase == 'package':
        accepted(root)
        inputs.append(root/'notary_submit.json')
        files.update(CHECKSUM=root/(name+'.sha256'),
                     MANIFEST=root/(name[:-4]+'.manifest.txt'))
        tools += ['xcrun', 'spctl']
    command = [sys.executable, '-B', str(Path(__file__).resolve()), '_assemble',
               '--root', str(root), '--version', version, '--phase', phase]
    return run(repo, root, {'REPORTS':root/('native-'+phase+'-reports')}, files, {},
               inputs, ['native-phase='+phase, 'version='+version], command, tools)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('upload', 'accepted', 'package', '_assemble'))
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--version', required=True)
    parser.add_argument('--phase', choices=('upload', 'package'))
    args, mappings = parser.parse_known_args()
    try:
        root = Path(os.path.abspath(args.root)); repo = Path.cwd().resolve()
        selected = identity(args.version)
        if args.action == '_assemble':
            if args.phase is None: raise ValueError('Assembly phase required')
            assemble(repo, root, selected, args.phase, parse_assignments(mappings))
        else:
            if mappings or args.phase: raise ValueError('Unexpected assembly arguments')
            if args.action == 'accepted': print(json.dumps(accepted(root)))
            else: print(json.dumps(archive(repo, root, args.version, args.action)))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, 'Native release held: '+str(error)+'\n')


if __name__ == '__main__':
    main()
