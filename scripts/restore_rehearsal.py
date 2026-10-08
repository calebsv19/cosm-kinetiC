"""Restore checksum-bound evidence archives into fresh roots; never over live state."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path, PurePosixPath
import tarfile
import time

from cfd_evidence import admitted_path, file_fingerprint, file_digest, regular_inventory, verify_bundle
from check_clean_root import read_json

PAYLOAD=frozenset(('RESTORE.md','fresh-lifecycle-evidence.tar.gz','historical-survivors.tar.gz','incident.md','repair-status.md','survivor-manifest.json'))


def member_path(name):
    value=PurePosixPath(name)
    if not name or value.is_absolute() or any(part in ('..','.') for part in name.split('/')) or '\\' in name:
        raise ValueError('Unsafe archive member path: '+name)
    return value


def extract(archive,destination,max_entries=25000,max_bytes=4294967296,wall_cap=300):
    archive=admitted_path(archive);destination=admitted_path(destination)
    destination.mkdir()  # Exclusive new identity, including refusal of empty predecessors.
    started=time.monotonic();seen=set();files={};total=0
    with tarfile.open(archive,'r|gz') as stream:
        for entry in stream:
            relative=member_path(entry.name)
            if entry.name in seen:raise ValueError('Duplicate archive member: '+entry.name)
            seen.add(entry.name)
            if len(seen)>max_entries or time.monotonic()-started>wall_cap:raise ValueError('Archive entry/time cap')
            if not (entry.isfile() or entry.isdir()):raise ValueError('Archive link or special member refused: '+entry.name)
            output=destination/relative
            if entry.isdir():output.mkdir(parents=True,exist_ok=True);continue
            if entry.size<0 or entry.size>1073741824 or total+entry.size>max_bytes:raise ValueError('Archive byte cap')
            output.parent.mkdir(parents=True,exist_ok=True);digest=hashlib.sha256();written=0
            incoming=stream.extractfile(entry)
            if incoming is None:raise ValueError('Missing archive data')
            descriptor=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
            with incoming,os.fdopen(descriptor,'wb') as target:
                while chunk:=incoming.read(1048576):
                    if time.monotonic()-started>wall_cap:raise ValueError('Archive wall cap')
                    written+=len(chunk)
                    if written>entry.size:raise ValueError('Archive data exceeds declared size')
                    target.write(chunk);digest.update(chunk)
                if written!=entry.size:raise ValueError('Truncated archive member')
                target.flush();os.fsync(target.fileno())
            # Preserve ordinary executable/read modes, never special permission bits.
            output.chmod(entry.mode & 0o777)
            total+=written;files[str(relative)]={'bytes':written,'sha256':digest.hexdigest()}
    for name,row in files.items():
        if file_digest(destination/name)!=row:raise ValueError('Restored artifact changed: '+name)
    return {'files':len(files),'logical_bytes':total,'inventory':files,'destination':str(destination)}


def rehearse(payload,receipt,destination):
    payload=admitted_path(payload);receipt=admitted_path(receipt);destination=admitted_path(destination)
    source_identity=file_fingerprint(receipt)
    source=read_json(receipt,65536)
    if destination.is_relative_to(payload) or payload.is_relative_to(destination):
        raise ValueError('Restore destination overlaps retrieved payload')
    if not isinstance(source,dict) or source.get('status')!='verified_independent_cold_archive_copy':
        raise ValueError('Expected verified independent copy receipt')
    if not isinstance(source.get('archive_destination'),str) or not source['archive_destination']:
        raise ValueError('Missing archive provenance')
    expected=source.get('payload_checksums')
    if not isinstance(expected,dict) or set(expected)!=PAYLOAD:raise ValueError('Unexpected prepared payload inventory')
    _,observed=regular_inventory(payload)
    if set(observed)!=PAYLOAD:raise ValueError('Retrieved payload has missing or extra files')
    if any(not isinstance(value,str) or not re.fullmatch('[0-9a-f]{64}',value) for value in expected.values()):
        raise ValueError('Invalid prepared payload checksums')
    identities={}
    for name,digest in expected.items():
        row=file_fingerprint(payload/name)
        if row['sha256']!=digest:raise ValueError('Retrieved payload checksum mismatch: '+name)
        identities[name]=row
    destination.mkdir()
    historical=extract(payload/'historical-survivors.tar.gz',destination/'historical')
    fresh=extract(payload/'fresh-lifecycle-evidence.tar.gz',destination/'fresh')
    survivors=read_json(payload/'survivor-manifest.json',16777216)
    if not isinstance(survivors,list) or not survivors:raise ValueError('Invalid survivor inventory')
    survivor_rows={}
    for row in survivors:
        if not isinstance(row,dict) or set(row)!= {'path','bytes','sha256'}:raise ValueError('Invalid survivor row')
        if (not isinstance(row['path'],str) or type(row['bytes']) is not int or row['bytes']<0
                or not isinstance(row['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',row['sha256'])):
            raise ValueError('Invalid survivor fields')
        name=str(member_path(row['path']))
        if name in survivor_rows:raise ValueError('Duplicate survivor path')
        survivor_rows[name]={'bytes':row['bytes'],'sha256':row['sha256']}
    if historical['inventory']!=survivor_rows:raise ValueError('Restored historical inventory differs from survivor manifest')
    bundles=[]
    for manifest in (destination/'fresh').rglob('bundle_manifest.json'):
        bundles.append({'bundle':str(manifest.parent.relative_to(destination/'fresh')),**verify_bundle(manifest.parent)})
    if not bundles:raise ValueError('Fresh archive contains no sealed evidence')
    for name,identity in identities.items():
        if file_fingerprint(payload/name)!=identity:raise ValueError('Retrieved payload changed during restoration')
    if file_fingerprint(receipt)!=source_identity:raise ValueError('Source copy receipt changed during restoration')
    result={'schema':'physics_sim_restore_rehearsal_v1','artifact_class':'retained_evidence',
        'status':'verified_independent_archive_retrieval_and_restore','archive_destination':source['archive_destination'],
        'payload_checksums':expected,'source_copy_receipt_sha256':file_digest(receipt)['sha256'],
        'historical_restored_files':historical['files'],'historical_restored_bytes':historical['logical_bytes'],
        'historical_manifest_matched':True,'fresh_restored_files':fresh['files'],'fresh_restored_bytes':fresh['logical_bytes'],
        'fresh_bundles_verified':bundles,'restore_root':str(destination),'originals_modified':False,
        'remote_archive_modified':False,'later_evidence_covered':False,'runtime_or_numerical_qualification_verified':False}
    (destination/'restore_receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('payload','receipt','destination'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args()
    try:print(json.dumps(rehearse(args.payload,args.receipt,args.destination),indent=2))
    except (OSError,ValueError,tarfile.TarError,RecursionError) as error:parser.exit(2,'Restore held; partial destination retained: '+str(error)+'\n')


if __name__=='__main__':main()
