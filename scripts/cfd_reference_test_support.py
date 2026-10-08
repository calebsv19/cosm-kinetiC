"""Declared tracked support-library identities for current macOS reference tests.

Resolving a path never builds a library or reads a historical experiment.
Make owns the compilation prerequisite; tests own their small numerical inputs.
"""
import argparse
import os
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
LEGACY_SOURCES = {
    'c3d-cholesky/support/factor.dylib': 'accelerate',
    'c3d-symbolic/support/symbolic.dylib': 'symbolic',
    'c3d-workspace-cholesky/support/factor.dylib': 'workspace_cholesky',
    'c3d-vector-storage/support/factor.dylib': 'vector_storage',
    'c3d-mixed-precision/support/factor.dylib': 'mixed_storage',
    'c3d-pruned-graph/support-factor.dylib': 'mixed_storage',
    'c3d-second-normal/scalar-support-factor.dylib': 'mixed_storage',
    'c3d-encoded-operator/support/factor.dylib': 'encoded_storage',
    'c3d-block-ic0/support/factor.dylib': 'block_ic0',
    'c3d-fillcomp-ic0/support/factor.dylib': 'fillcomp_ic0',
    'c3d-bounded-fill1/support/factor.dylib': 'bounded_fill1',
    'c3d-distributed-p2/support/coarse.dylib': 'distributed_p2_coarse',
    'c3d-distributed-p3/support/coarse.dylib': 'distributed_p3_coarse',
    'c3d-p3-cg8-scalar/support/coarse.dylib': 'p3_coarse_scalar',
    'c3d-native-inner8/support/factor.dylib': 'native_inner8',
    'c3d-packed-inner8/support/factor.dylib': 'packed_inner8',
    'c3d-packed-physical/support/factor.dylib': 'packed_physical',
    'c3d-local-cost-split/support/factor.dylib': 'local_cost_split',
    'c3d-seed-fill1/support/factor.dylib': 'seed_fill1',
    'c3d-priority-fill1/support/factor.dylib': 'priority_fill1',
    'c3d-tight-fill-bound/support/factor.dylib': 'tight_fill_bound',
}


def library_path(legacy):
    key = legacy.removeprefix('build/')
    name = LEGACY_SOURCES[key]
    if name == 'accelerate' and os.environ.get('CFD_REFERENCE_FACTOR_LIBRARY'):
        return Path(os.environ['CFD_REFERENCE_FACTOR_LIBRARY'])
    root = Path(os.environ.get('PHYSICS_SIM_BUILD_ROOT', REPO/'build'))
    return root/'cfd-reference-support'/('cfd_reference3d_'+name+'.dylib')


def archive_root():
    value = os.environ.get('PHYSICS_SIM_CFD_ARCHIVE_ROOT')
    if not value:
        raise ValueError('Set PHYSICS_SIM_CFD_ARCHIVE_ROOT to an explicit historical archive root (contents formerly beneath build/)')
    path = Path(value).resolve()
    if not path.is_dir():
        raise ValueError('Historical archive root is missing: '+str(path))
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stems', action='store_true', required=True)
    parser.parse_args()
    print(' '.join('cfd_reference3d_'+name for name in sorted(set(LEGACY_SOURCES.values()))))


if __name__ == '__main__':
    main()
