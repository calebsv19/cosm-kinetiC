"""Require complete, fresh MEW1 absence evidence before local app replacement."""
import argparse
import json
import math
import os
from pathlib import Path
import time

from check_clean_root import read_json
from clean_outputs import no_symlinks


def validate(repo, receipt, match, destination, max_age=30):
    repo=repo.resolve();receipt=Path(os.path.abspath(receipt))
    no_symlinks(receipt,repo)
    if not math.isfinite(max_age) or max_age<=0:raise ValueError('Process audit age limit must be positive and finite')
    before=receipt.stat()
    row=read_json(receipt,1024*1024)
    after=receipt.stat()
    if (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns)!=(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns):
        raise ValueError('Process audit changed during validation')
    age=time.time()-after.st_mtime
    if age<0 or age>max_age:raise ValueError('Process audit is stale or has a future timestamp')
    if not isinstance(row,dict) or row.get('schema_version')!='codework_mew1_process_audit_v1':
        raise ValueError('Expected a typed MEW1 process audit')
    required={'schema_version','match','path','method','running','matches','truncated'}
    if not required<=row.keys() or 'error' in row:
        raise ValueError('Process audit is incomplete or reports an error')
    if row['match']!=match or row['path']!=str(destination.resolve()):
        raise ValueError('Process audit does not bind the selected app name and destination')
    if type(row['running']) is not bool or type(row['truncated']) is not bool or not isinstance(row['matches'],list):
        raise ValueError('Process audit state has invalid field types')
    if row['truncated']:raise ValueError('Process audit is truncated')
    if row['running'] or row['matches']:raise ValueError('Desktop refresh held by running or matched processes')
    # With an explicit path, MEW1 performs lsof fallback when the literal scan
    # is empty. ps_literal alone cannot establish this selected-path absence.
    if row['method']!='lsof_path_fallback':raise ValueError('Process audit did not complete selected-path fallback')
    return {'status':'verified_absence','receipt':str(receipt),'destination':row['path'],
            'observation_age_seconds':age,'scope':'bounded MEW1 process/path observation; no termination or release authority'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt',type=Path,required=True)
    parser.add_argument('--match',required=True)
    parser.add_argument('--path',type=Path,required=True)
    parser.add_argument('--max-age-seconds',type=float,default=30)
    args=parser.parse_args()
    try:print(json.dumps(validate(Path.cwd(),args.receipt,args.match,args.path,args.max_age_seconds)))
    except (ValueError,OSError,RecursionError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
