"""Bounded read-only retention inventory. Never grants pruning or deletes artifacts."""
import argparse
import json
import os
import math
from pathlib import Path
import stat
import time

from cfd_evidence import admitted_path, sha
from check_clean_root import read_json, evidence_markers, HELD_DIRECTORIES

MARKERS=frozenset(('request.json','receipt.json','fixture_owner.json','.reference-setup.json','artifact_owner.json','fixture_terminal.json'))


def policy(repo):
    path=repo/'config/artifact_retention_policy.json';row=read_json(path,16384)
    if (not isinstance(row,dict) or row.get('schema')!='physics_sim_retention_policy_v1'
            or type(row.get('version')) is not int or row['version']<1
            or row.get('age_alone_permits_pruning') is not False or not isinstance(row.get('classes'),dict)):
        raise ValueError('Invalid retention policy')
    for name,rule in row['classes'].items():
        if not isinstance(rule,dict) or any(not isinstance(rule.get(k),str) or not rule[k] for k in ('lifetime','route')):
            raise ValueError('Invalid retention class '+name)
    if not {'unclassified','retained_evidence','tool_storage','disposable_build'}<=set(row['classes']):
        raise ValueError('Missing required hold policy classes')
    return row,sha(path)


def audit(repo,path,rules,max_entries=100000,wall_cap=5):
    repo=repo.resolve();path=admitted_path(path if path.is_absolute() else repo/path)
    allowed=[repo/name for name in ('build','tmp','data','visual_artifacts','export','dist')]
    if not any(path==root or path.is_relative_to(root) for root in allowed):
        raise ValueError('Retention inventory must select a generated/retained checkout namespace')
    if type(max_entries) is not int or not 1<=max_entries<=1000000 or type(wall_cap) not in (int,float) or not math.isfinite(wall_cap) or not 0<wall_cap<=30:
        raise ValueError('Invalid retention inventory bound')
    result={'path':str(path),'exists':path.exists(),'regular_files':0,'logical_bytes':0,'directories':0,
        'symlinks':0,'special_files':0,'metadata_records':[],'diagnostics':[],'inventory_complete':True,
        'classification_is_ownership_proof':False,'observed_classes':[],'integrity_verified':False,'archive_coverage_verified':False,
        'terminal_ownership_verified':False,'snapshot_consistency_verified':False,'eligible_for_pruning':False}
    if not path.exists():return {**result,'status':'absent','actions':[]}
    started=time.monotonic();pending=[path];entries=0;classes=set()
    def diagnostic(current,reason):
        if len(result['diagnostics'])<20:result['diagnostics'].append({'path':str(current),'reason':reason})
    while pending:
        if entries>=max_entries or time.monotonic()-started>=wall_cap:
            result['inventory_complete']=False;diagnostic(path,'Inventory entry/time bound reached');break
        current=pending.pop();entries+=1
        try:
            admitted_path(current.parent)
            info=current.lstat()
            if stat.S_ISLNK(info.st_mode):result['symlinks']+=1;continue
            admitted_path(current)
            if stat.S_ISDIR(info.st_mode):
                result['directories']+=1
                if current.name in HELD_DIRECTORIES:classes.add('retained_evidence')
                # The iterator itself is bounded: never materialize a huge directory.
                descriptor=os.open(current,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
                try:
                    selected=os.fstat(descriptor)
                    if (selected.st_dev,selected.st_ino)!=(info.st_dev,info.st_ino):
                        raise ValueError('Directory changed during admission')
                    with os.scandir(descriptor) as children:
                        for child in children:
                            if entries+len(pending)>=max_entries or time.monotonic()-started>=wall_cap:
                                result['inventory_complete']=False;diagnostic(current,'Directory enumeration bound reached');break
                            pending.append(current/child.name)
                finally:os.close(descriptor)
                continue
            if not stat.S_ISREG(info.st_mode):result['special_files']+=1;diagnostic(current,'Special file held');continue
            result['regular_files']+=1;result['logical_bytes']+=info.st_size
            if current.name in ('bundle_manifest.json','field.bin','.physics-sim-headless-owner') or current.suffix in ('.npy','.npz'):
                classes.add('retained_evidence')
            if current.name in ('.physics-sim-job-metadata.lock','.physics-sim-job-operation.lock'):classes.add('operational_job')
            if current.name.startswith('.headless-sidecar-') and current.name.endswith('.pending'):classes.add('retained_evidence')
            if current.name in MARKERS:
                row=read_json(current,65536)
                if not isinstance(row,dict):raise ValueError('Retention marker must be an object')
                if evidence_markers(row):classes.add('retained_evidence')
                declared=row.get('artifact_class')
                if declared is not None:
                    if not isinstance(declared,str) or declared not in rules['classes']:
                        raise ValueError('Unrecognized artifact class')
                    classes.add(declared)
                if len(result['metadata_records'])<100:
                    result['metadata_records'].append({'path':str(current),'declared_class':declared,
                        'declared_state':row.get('status',row.get('state')),'ownership_verified':False})
        except (OSError,ValueError,RecursionError) as error:
            result['inventory_complete']=False;diagnostic(current,str(error))
    if path.is_relative_to(repo/'build'):classes.add('disposable_build')
    if path.is_relative_to(repo/'data/tools'):classes.add('tool_storage')
    if not classes:classes.add('unclassified')
    result['observed_classes']=sorted(classes)
    result['actions']=[{'artifact_class':name,**rules['classes'][name]} for name in sorted(classes)]
    result['status']='held' if result['inventory_complete'] else 'incomplete_held'
    result['entries_observed']=entries
    if 'disposable_build' in classes and len(classes)>1:diagnostic(path,'Mixed build and retained classes; ordinary clean must hold')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--path',type=Path,action='append',required=True)
    parser.add_argument('--max-entries',type=int,default=100000);parser.add_argument('--wall-cap',type=float,default=5)
    args=parser.parse_args()
    try:
        repo=Path.cwd().resolve();rules,digest=policy(repo)
        rows=[audit(repo,path,rules,args.max_entries,args.wall_cap) for path in args.path]
        print(json.dumps({'schema':'physics_sim_retention_audit_v1','policy_version':rules['version'],
            'policy_sha256':digest,'mutations_performed':[],'pruning_authorized':False,'inventory':rows},indent=2))
        raise SystemExit(2 if any(not row['inventory_complete'] for row in rows) else 0)
    except (OSError,ValueError,RecursionError) as error:parser.exit(2,'Retention audit held: '+str(error)+'\n')


if __name__=='__main__':main()
