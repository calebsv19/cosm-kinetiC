"""Admit fresh local package outputs; retain every predecessor and failed attempt.

This is source-level path/lifetime admission, not release or installation authority.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import uuid

from package_paths import no_symlinks, bound_root


def plan(repo, root, directories, files):
    repo=repo.resolve()
    if ".." in Path(root).parts or any(".." in Path(value).parts for value in [*directories, *files]):
        raise ValueError("Package path contains traversal")
    root=Path(os.path.abspath(root))
    no_symlinks(root,repo)
    if not (root.is_relative_to(repo/'build') and root!=repo/'build' or root==repo/'dist' or root.is_relative_to(repo/'dist') or not root.is_relative_to(repo) and root.is_relative_to(bound_root(root,repo))):
        raise ValueError('Package outputs require a strict build descendant or dist namespace')
    if root.is_relative_to(repo/'build') and any(part in ('bin','clang','fisics','cfd-reference-support') for part in root.relative_to(repo/'build').parts):
        raise ValueError('Package namespace overlaps compiler outputs')
    if root.exists() and not root.is_dir():raise ValueError('Package root is not a directory')
    paths=[]
    for kind,values in (('directory',directories),('file',files)):
        for value in values:
            path=Path(os.path.abspath(value));no_symlinks(path,repo)
            if not path.is_relative_to(root):raise ValueError('Package output escapes declared root: '+str(path))
            if path in [Path(row['path']) for row in paths]:raise ValueError('Duplicate package output: '+str(path))
            reservation=root/'.package-reservations'/(hashlib.sha256(str(path).encode()).hexdigest()+'.json')
            no_symlinks(reservation,repo)
            paths.append({'path':str(path),'kind':kind,'exists':path.exists(),
                          'reservation':str(reservation),'reserved':reservation.exists()})
    if not paths:raise ValueError('At least one exact output is required')
    # Directories can contain their own declared children, but file/parent ambiguity is refused.
    for row in paths:
        if row['kind']=='file' and any(Path(other['path']).is_relative_to(row['path']) for other in paths if other!=row):
            raise ValueError('Package file also used as an output parent')
    return {'schema':'physics_sim_package_output_plan_v1','root':str(root),'outputs':paths,
            'fresh':not any(row['exists'] or row['reserved'] for row in paths),'predecessor_removal_permitted':False}


def declare(planned, validate):
    if not planned['fresh']:raise ValueError('Existing package outputs retained; use a fresh job-scoped attempt through release control')
    if validate()!=planned:raise ValueError('Package output plan changed before declaration')
    invocation=str(uuid.uuid4())
    for row in sorted(planned['outputs'],key=lambda r:len(Path(r['path']).parts)):
        if row['kind']!='directory':continue
        path=Path(row['path']);path.parent.mkdir(parents=True,exist_ok=True);path.mkdir()
    for row in planned['outputs']:
        reservation=Path(row['reservation']);reservation.parent.mkdir(parents=True,exist_ok=True)
        with reservation.open('x') as stream:
            json.dump({'schema':'physics_sim_artifact_owner_v1','artifact_class':'local_package_staging',
                       'attempt_id':invocation,'root':planned['root'],'output':row['path'],
                       'state':'fresh_reserved_attempt','release_authority_granted':False},stream,indent=2)
            stream.write('\n')
    return dict(planned,status='fresh_outputs_admitted',attempt_id=invocation)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',required=True,type=Path)
    parser.add_argument('--directory',action='append',default=[],type=Path)
    parser.add_argument('--file',action='append',default=[],type=Path)
    parser.add_argument('--declare',action='store_true')
    parser.add_argument('--allocate')
    args=parser.parse_args()
    if args.allocate:
        if args.directory or args.file or not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}',args.allocate):
            parser.error('--allocate requires a simple name and no explicit outputs')
        args.directory=[args.root/(args.allocate+'-'+uuid.uuid4().hex)]
        args.declare=True
    validate=lambda:plan(Path.cwd(),args.root,args.directory,args.file)
    try:
        planned=validate()
        if not planned['fresh']:
            raise ValueError('Existing package outputs retained; no reset performed: '+', '.join(row['path'] for row in planned['outputs'] if row['exists'] or row['reserved']))
        result=declare(planned,validate) if args.declare else planned
        print(result['outputs'][0]['path'] if args.allocate else json.dumps(result,indent=2))
    except (ValueError,OSError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
