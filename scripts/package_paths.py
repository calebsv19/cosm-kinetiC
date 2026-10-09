"""Package-only admission of exact Registry-bound output namespaces.

Immutable contract bindings identify paths, not release/cleanup authority.
Compilation cleanup retains its checkout-only admission.
"""
import hashlib
import json
from pathlib import Path
import re

from clean_outputs import no_symlinks as checkout_no_symlinks

DATA_ROOT = Path.home() / 'CodeWorkData'
CONTRACTS = 'production_registry/control/release_authorization_precommit_preparations/contracts/sha256'


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                    ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def bound_root(path, repo):
    """Return the exact package-target root, refusing every broader namespace."""
    data = DATA_ROOT
    if not data.is_absolute() or data.is_symlink() or not path.is_relative_to(data):
        raise ValueError('Package path is outside the bound data root')
    parts = path.relative_to(data).parts
    if (len(parts) < 6 or parts[:3] != ('physics_sim', 'build', 'release-authenticated')
            or not re.fullmatch(r'raor_[0-9a-f]{64}', parts[3])
            or parts[4] != 'targets' or not re.fullmatch(r'rapt_[0-9a-f]{64}', parts[5])):
        raise ValueError('Package path is not an exact bound target namespace')
    selected = '/'.join(parts[:4])
    contracts = data / CONTRACTS
    current = contracts
    while current != data:
        if current.is_symlink():
            raise ValueError('Package contract store contains a symlink')
        current = current.parent
    for record in contracts.glob('*.json'):
        if record.is_symlink() or record.stat().st_size > 4 * 1024 * 1024:
            continue
        try:
            value = json.loads(record.read_text())
            if digest(value) != record.stem:
                continue
            roots = value['source_data_roots']
            owner = value['owner_adapter_binding']
            binding = value['output_root_binding']
            if (value['schema_version'] != 'production-registry/release-authorization-precommit-preparation-contract/v1'
                    or value['program'] != 'physics_sim'
                    or roots['source_workspace_root'] != str(repo.parent)
                    or roots['data_workspace_root'] != str(data)
                    or owner['repository_path'] != repo.name or repo.name != 'physics_sim'
                    or binding['selected_root'] != selected
                    or binding['candidate_scope_id'] != parts[3]):
                continue
            targets = {'rapt_' + digest({'package_target': target})
                       for target in owner['package_targets']}
            if parts[5] in targets:
                return data.joinpath(*parts[:6])
        except (KeyError, TypeError, ValueError, OSError):
            continue
    raise ValueError('Package target has no matching immutable source/data contract')


def no_symlinks(path, repo):
    if '..' in Path(path).parts:
        raise ValueError('Package path contains traversal')
    if path.is_relative_to(repo):
        return checkout_no_symlinks(path, repo)
    bound_root(path, repo)
    current = path
    while True:
        if current.is_symlink():
            raise ValueError('Package refuses symlink component: ' + str(current))
        if current == current.parent:
            return
        current = current.parent


def reservation_output(repo, receipt, row):
    """Keep local cleanup admission intact; validate exact external package ownership."""
    from check_clean_root import reservation_output as local_reservation_output
    import uuid
    if receipt.is_relative_to(repo):
        return local_reservation_output(repo, receipt, row)
    invalid = 'Invalid bound package output reservation'
    root = receipt.parent.parent
    if bound_root(root, repo) != root or receipt.parent.name != '.package-reservations':
        raise ValueError(invalid + ' namespace')
    expected = {'schema', 'artifact_class', 'attempt_id', 'output', 'state',
                'root', 'release_authority_granted'}
    if (not isinstance(row, dict) or set(row) != expected
            or row['schema'] != 'physics_sim_artifact_owner_v1'
            or row['artifact_class'] != 'local_package_staging'
            or row['state'] != 'fresh_reserved_attempt'
            or row['root'] != str(root) or row['release_authority_granted'] is not False):
        raise ValueError(invalid + ' schema/root/authority')
    try:
        attempt = uuid.UUID(row['attempt_id'])
        if attempt.version != 4 or str(attempt) != row['attempt_id']:
            raise ValueError(invalid + ' attempt')
    except (ValueError, TypeError, AttributeError):
        raise ValueError(invalid + ' attempt') from None
    value = row['output']
    if not isinstance(value, str) or not value or len(value) > 4096 or any(ord(c) < 32 for c in value):
        raise ValueError(invalid + ' output')
    output = Path(value)
    if (not output.is_absolute() or str(output) != value or '..' in output.parts
            or not output.is_relative_to(root) or output == root
            or receipt.name != hashlib.sha256(value.encode()).hexdigest() + '.json'):
        raise ValueError(invalid + ' output identity')
    no_symlinks(receipt, repo)
    no_symlinks(output, repo)
    return output
