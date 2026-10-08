"""Allocate one fresh fixture-owned directory; never reset a caller's parent."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import tempfile
import uuid

from clean_outputs import no_symlinks
from fixture_session import context, save
from cfd_evidence import sha


def allocate(repo, parent, name, retained=False):
    repo=repo.resolve();parent=Path(os.path.abspath(parent))
    no_symlinks(parent,repo)
    allowed=(repo/'visual_artifacts',repo/'data/experiments') if retained else (repo/'tmp',)
    if not any(parent==root or parent.is_relative_to(root) for root in allowed):
        raise ValueError('Fixture parent must be in '+', '.join(str(p) for p in allowed))
    if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}',name):
        raise ValueError('Invalid fixture name')
    parent.mkdir(parents=True,exist_ok=True)
    capsule=Path(tempfile.mkdtemp(prefix=name+'-',dir=parent))
    directory=capsule/'work';directory.mkdir()
    owner={'schema':'physics_sim_fixture_owner_v1','artifact_class':'fixture_visual_output' if retained else 'fixture_scratch',
           'fixture':name,'invocation_id':str(uuid.uuid4()),'created_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'root':str(directory),'capsule_root':str(capsule),'parent_reset_performed':False,'automatic_removal':False}
    session=context(repo)
    owner['lifetime_supervised']=session is not None
    if session:
        session_directory,descriptor,request=session
        owner.update(session_id=request['session_id'],session_dir=str(session_directory),status='running')
    else:owner['status']='allocated_unsupervised'
    save(capsule/'fixture_owner.json',owner)
    if session:
        save(session_directory/('capsule-'+owner['invocation_id']+'.json'),
             {'capsule':str(capsule),'invocation_id':owner['invocation_id'],'owner_sha256':sha(capsule/'fixture_owner.json')})
    return directory


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',required=True,type=Path)
    parser.add_argument('--parent',required=True,type=Path)
    parser.add_argument('--name',required=True)
    parser.add_argument('--retained',action='store_true')
    args=parser.parse_args()
    try:
        directory=allocate(args.repo,args.parent,args.name,args.retained)
        print(directory)
    except (ValueError,OSError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
