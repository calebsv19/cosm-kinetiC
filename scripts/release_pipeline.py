"""Receipt-bound local release phases; effect authority belongs to Release Control."""
import argparse
import json
import os
from pathlib import Path

from package_outputs import plan
from release_app_stage import stage
from release_notary_archive import prepare as archive_prepare
from release_notary import run as notary_run
from release_final_artifact import prepare as final_prepare, identity_fields, bundle_identity

ACTIONS = ('sign', 'notarize', 'staple', 'artifact')


def run(repo, source, root, action, identity, signing_identity, profile, tools, archive_name):
    repo = repo.resolve()
    source = Path(os.path.abspath(source))
    root = Path(os.path.abspath(root))
    identity_fields(identity)
    if set(tools) != {'codesign', 'xcrun', 'spctl', 'lipo', 'ditto'}:
        raise ValueError('Release pipeline requires exact tool selections')
    bundle_identity(source, identity)
    if action not in ACTIONS:
        raise ValueError('Unknown release phase')
    if source.is_relative_to(root) or root.is_relative_to(source):
        raise ValueError('Release pipeline root overlaps source app')
    if action != 'sign' and (not profile or signing_identity == '-'):
        raise ValueError('Notarized phases require Developer ID identity and keychain profile')
    # Admit the complete namespace before allocating or transforming anything.
    roots = {name: root / name for name in ('signed', 'upload', 'notary', 'stapled', 'final')}
    for selected in roots.values():
        plan(repo, selected, [selected / 'command-reports'], [])
    signed = stage(repo, source, roots['signed'], identity['bundle_id'],
                   'sign', tools['codesign'], signing_identity)
    if action == 'sign':
        return signed
    upload = archive_prepare(repo, Path(signed['receipt']), roots['upload'], tools['ditto'])
    journal = roots['notary'] / 'receipt.json'
    arguments = (repo, roots['upload'] / 'notary-upload.zip', roots['notary'],
                 Path(upload['receipt']), profile, tools['xcrun'])
    existed = journal.exists()
    state = notary_run(*arguments, reconcile=existed)
    # A new submission gets one explicit same-ID query. Subsequent invocations
    # only reconcile that journal; never repeat the submit or poll in background.
    if not existed and state['state'] == 'submitted':
        state = notary_run(*arguments, reconcile=True)
    if state['state'] != 'accepted':
        raise ValueError('Notarization is ' + state['state'] +
                         '; retained journal: ' + str(journal) + '; rerun to query the same ID')
    if action == 'notarize':
        return {'status': 'accepted', 'receipt': str(journal), 'submission_id': state['submission_id']}
    stapled = stage(repo, roots['signed'] / source.name, roots['stapled'], identity['bundle_id'],
                    'staple', tools['xcrun'], notary_receipt=journal)
    if action == 'staple':
        return stapled
    return final_prepare(repo, Path(stapled['receipt']), roots['final'], identity,
                         tools=tools, archive_name=archive_name)


def assignments(items):
    result = {}
    for item in items:
        key, separator, value = item.partition('=')
        if not separator or key in result:
            raise ValueError('Invalid or duplicate assignment')
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=ACTIONS)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--identity', action='append', default=[])
    parser.add_argument('--tool', action='append', default=[])
    parser.add_argument('--signing-identity', default='-')
    parser.add_argument('--profile', default='')
    parser.add_argument('--archive-name', required=True)
    args = parser.parse_args()
    try:
        tools = {'codesign': 'codesign', 'xcrun': 'xcrun', 'spctl': 'spctl',
                 'lipo': 'lipo', 'ditto': '/usr/bin/ditto'}
        selected = assignments(args.tool)
        if set(selected) - set(tools):
            raise ValueError('Unknown release tool')
        tools.update(selected)
        print(json.dumps(run(Path.cwd(), args.source, args.root, args.action,
                             assignments(args.identity), args.signing_identity,
                             args.profile, tools, args.archive_name)))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, 'Release pipeline held: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
