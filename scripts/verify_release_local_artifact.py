"""Verify unsigned local ZIP identity metadata before transaction publication."""
import argparse
import hashlib
import os
from pathlib import Path
import stat

from cfd_evidence import admitted_path
from desktop_replace import inventory


def metadata(path, expected):
    descriptor=os.open(admitted_path(path),os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):raise ValueError('Artifact metadata must be regular')
        with os.fdopen(descriptor,'rb',closefd=False) as stream:payload=stream.read(65537)
        if len(payload)>65536:raise ValueError('Artifact metadata exceeds bound')
        if hashlib.sha256(payload).hexdigest()!=expected['sha256']:raise ValueError('Artifact metadata changed during readback')
        return payload.decode()
    finally:os.close(descriptor)


def verify(archive, checksum, manifest):
    rows={path:inventory(path)['.'] for path in (archive,checksum,manifest)}
    if any(row['kind']!='file' for row in rows.values()):raise ValueError('Artifact outputs must be regular')
    digest=rows[archive]['sha256']
    if metadata(checksum,rows[checksum])!=digest+'  '+archive.name+'\n':
        raise ValueError('Artifact checksum or basename mismatch')
    values={}
    for line in metadata(manifest,rows[manifest]).splitlines():
        key,separator,value=line.partition('=')
        if not separator or key in values or not value:raise ValueError('Invalid or duplicate artifact manifest field')
        values[key]=value
    required={'product','program','version','platform','arch','format','channel','artifact','sha256','signed','notarized'}
    if set(values)!=required:raise ValueError('Artifact manifest fields mismatch')
    for key,value in {'format':'zip','artifact':archive.name,'sha256':digest,'signed':'false','notarized':'false'}.items():
        if values[key]!=value:raise ValueError('Artifact manifest '+key+' mismatch')
    if any(inventory(path)['.']!=row for path,row in rows.items()):raise ValueError('Artifact outputs changed during verification')
    return digest


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('archive','checksum','manifest'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    try:print('Unsigned local artifact identity verified: '+verify(args.archive,args.checksum,args.manifest))
    except (ValueError,OSError) as error:parser.exit(2,str(error)+'\n')

if __name__=='__main__':main()
