"""Retained final artifact production from an exact accepted/stapled app.

Owning release authority remains external; no Registry/public promotion occurs.
"""
import argparse
import fcntl
import json
import hashlib
import stat
import os
from pathlib import Path
import plistlib
import sys

from check_clean_root import read_json
from contract_proof import execute
from desktop_replace import check_bundle, inventory, replace as replace_desktop
from package_paths import no_symlinks
from package_outputs import plan, declare
from package_transaction import run, parse_assignments
from release_notary import completed, values, accepted_binding, receipt_digest
from release_zip_validation import verify as verify_zip
from verify_release_local_artifact import metadata as read_metadata

IDENTITY_FIELDS = {'product', 'program', 'bundle_id', 'version', 'channel', 'platform', 'arch'}


def identity_fields(selected):
    if set(selected) != IDENTITY_FIELDS:
        raise ValueError('Final artifact requires exact identity fields')
    if any(not isinstance(value, str) or not value or any(c in value for c in '\r\n\x00') for value in selected.values()):
        raise ValueError('Invalid artifact identity value')
    if selected['program'] != 'physics_sim' or selected['platform'] not in ('macos', 'macOS') or selected['arch'] not in ('arm64', 'x86_64'):
        raise ValueError('Unsupported PhysicsSim final artifact platform identity')


def stapled_input(repo, receipt):
    record = completed(repo, receipt); selected = values(record)
    if selected.get('phase') != 'staple': raise ValueError('Final artifact requires completed stapling stage')
    outputs = {item['name']: Path(record['output_root']) / item['relative'] for item in record['contract']['outputs']}
    if set(outputs) != {'APP_DEST', 'APP_REPORTS'}: raise ValueError('Invalid stapling output contract')
    stage = read_json(outputs['APP_REPORTS'] / 'stage.json', 1048576)
    if not isinstance(stage,dict) or stage.get('phase') != 'staple' or stage.get('tool_checks_completed') is not True:
        raise ValueError('Stapling verification evidence missing')
    previous = stage.get('notary_acceptance')
    if not isinstance(previous, dict): raise ValueError('Stapling lacks exact notarization acceptance')
    notary_receipt = Path(previous['inputs'][0]); signed = Path(previous['binding']['signed_app'])
    accepted = accepted_binding(repo, notary_receipt, signed)
    if accepted != previous or selected.get('notary_acceptance_sha256') != accepted['receipt_sha256']:
        raise ValueError('Stapling acceptance evidence changed')
    source = record['source_identity']['inputs']
    if str(signed) not in source or str(notary_receipt) not in source:
        raise ValueError('Stapling source contract lacks acceptance inputs')
    return outputs['APP_DEST'], accepted


def bundle_identity(app, selected):
    check_bundle(app, selected['bundle_id'])
    path = app / 'Contents/Info.plist';row=inventory(path)['.']
    descriptor=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        if not stat.S_ISREG(os.fstat(descriptor).st_mode):raise ValueError('Bundle plist must be regular')
        with os.fdopen(descriptor,'rb',closefd=False) as stream:payload=stream.read(65537)
        if len(payload)>65536 or hashlib.sha256(payload).hexdigest()!=row['sha256']:
            raise ValueError('Bundle plist changed or exceeds metadata bound')
    finally:os.close(descriptor)
    info=plistlib.loads(payload)
    if info.get('CFBundleShortVersionString') != selected['version'] or info.get('CFBundleName') != selected['product']:
        raise ValueError('Final artifact version/product differs from bundle metadata')


def verify_sidecars(archive, checksum, manifest, notary, selected, accepted, zip_proof):
    rows={path:inventory(path)['.'] for path in (checksum,manifest,notary)}
    if read_metadata(checksum,rows[checksum]) != zip_proof['archive_sha256']+'  '+archive.name+'\n':
        raise ValueError('Final checksum or portable basename mismatch')
    fields={}
    for line in read_metadata(manifest,rows[manifest]).splitlines():
        key,separator,value=line.partition('=')
        if not separator or not value or key in fields:raise ValueError('Invalid final artifact manifest')
        fields[key]=value
    expected=dict(selected,format='zip',signed='1',notarized='1',artifact=archive.name,
                  sha256=zip_proof['archive_sha256'],sha256_file=checksum.name,notary_json=notary.name)
    if fields!=expected:raise ValueError('Final manifest identity mismatch')
    evidence=read_json(notary,65536)
    if (evidence.get('id')!=accepted['submission_id'] or evidence.get('status')!='Accepted'
            or evidence.get('submitted_archive_sha256')!=accepted['binding']['archive_sha256']
            or evidence.get('final_archive_sha256')!=zip_proof['archive_sha256']
            or evidence.get('notary_acceptance_receipt_sha256')!=accepted['receipt_sha256']):
        raise ValueError('Final notary sidecar binding mismatch')
    if any(inventory(path)['.']!=row for path,row in rows.items()):
        raise ValueError('Final artifact sidecars changed during readback')


def assemble(repo, receipt, selected, tools, assignments):
    identity_fields(selected)
    if set(assignments)!={'RELEASE_ROOT','RELEASE_DIR','ARCHIVE','CHECKSUM','MANIFEST','NOTARY','REPORTS'}:
        raise ValueError('Final artifact requires exact transaction mappings')
    root=assignments['RELEASE_ROOT']
    if assignments['RELEASE_DIR']!=root:raise ValueError('Final artifact root mapping mismatch')
    app,accepted=stapled_input(repo,receipt);bundle_identity(app,selected)
    if app.is_relative_to(root) or root.is_relative_to(app) or receipt.is_relative_to(root):
        raise ValueError('Final artifact overlaps its stapling input')
    reports=assignments['REPORTS'];archive=assignments['ARCHIVE']
    files=[assignments[key] for key in ('ARCHIVE','CHECKSUM','MANIFEST','NOTARY')]
    validation=lambda:plan(repo,root,[reports],files)
    declared=validation();declare(declared,validation)
    def command(argv,tag):return execute(argv,repo,reports,tag,(),300,1048576)
    command([tools['codesign'],'--verify','--deep','--strict',str(app)],'codesign')
    command([tools['xcrun'],'stapler','validate',str(app)],'stapler')
    # A subsystem error or unnotarized response is a failure here, never acceptance.
    command([tools['spctl'],'--assess','--type','execute','--verbose=2',str(app)],'gatekeeper')
    rows=inventory(app)
    targets=[name for name,row in sorted(rows.items()) if row['kind']=='file' and
             (name in ('Contents/MacOS/physics-sim-bin','Contents/MacOS/physics_sim_session_worker','Contents/MacOS/physics-sim-launcher')
              or name.startswith('Contents/Frameworks/') and name.endswith('.dylib'))]
    for index,name in enumerate(targets):
        tag='architecture_'+str(index);command([tools['lipo'],'-archs',str(app/name)],tag)
        output=read_metadata(reports/(tag+'.stdout'),inventory(reports/(tag+'.stdout'))['.'])
        if output.split()!=[selected['arch']]:raise ValueError('Final bundle architecture mismatch: '+name)
    command([tools['ditto'],'-c','-k','--sequesterRsrc','--keepParent',str(app),str(archive)],'archive')
    zip_proof=verify_zip(archive,app)
    checksum=assignments['CHECKSUM'];manifest=assignments['MANIFEST'];notary=assignments['NOTARY']
    with checksum.open('x') as stream:stream.write(zip_proof['archive_sha256']+'  '+archive.name+'\n')
    fields=dict(selected,format='zip',signed='1',notarized='1',artifact=archive.name,
                sha256=zip_proof['archive_sha256'],sha256_file=checksum.name,notary_json=notary.name)
    with manifest.open('x') as stream:
        for key,value in fields.items():stream.write(key+'='+value+'\n')
    with notary.open('x') as stream:
        json.dump({'id':accepted['submission_id'],'status':'Accepted',
                   'submitted_archive_sha256':accepted['binding']['archive_sha256'],
                   'final_archive_sha256':zip_proof['archive_sha256'],
                   'notary_acceptance_receipt_sha256':accepted['receipt_sha256'],
                   'release_authority_granted':False,'public_or_registry_acceptance_verified':False},stream,indent=2)
        stream.write('\n')
    verify_sidecars(archive,checksum,manifest,notary,selected,accepted,zip_proof)
    if stapled_input(repo,receipt)!=(app,accepted):raise ValueError('Stapling evidence changed during final export')
    with (reports/'verification.json').open('x') as stream:
        json.dump({'zip':zip_proof,'gatekeeper_command_succeeded':True,
                   'architecture':selected['arch'],'public_or_registry_acceptance_verified':False},stream,indent=2)
        stream.write('\n')


def prepare(repo, receipt, root, selected, *, tools=None, archive_name='artifact.zip'):
    receipt=Path(os.path.abspath(receipt));root=Path(os.path.abspath(root));identity_fields(selected)
    if (not archive_name.endswith('.zip') or Path(archive_name).name!=archive_name
            or any(c in archive_name for c in '\r\n\x00')):raise ValueError('Final archive requires a simple ZIP name')
    app,accepted=stapled_input(repo,receipt);bundle_identity(app,selected)
    if app.is_relative_to(root) or root.is_relative_to(app) or receipt.is_relative_to(root):
        raise ValueError('Final artifact root overlaps input')
    tools=dict(tools or {'codesign':'codesign','xcrun':'xcrun','spctl':'spctl','lipo':'lipo','ditto':'/usr/bin/ditto'})
    if set(tools)!={'codesign','xcrun','spctl','lipo','ditto'}:raise ValueError('Invalid final verification tools')
    helper=Path(__file__).resolve();command=[sys.executable,'-B',str(helper),'_assemble','--staple-receipt',str(receipt)]
    for key,value in selected.items():command+=['--identity',key+'='+value]
    for key,value in tools.items():command+=['--tool',key+'='+value]
    return run(repo,root,{'REPORTS':root/'command-reports'},
               {'ARCHIVE':root/archive_name,'CHECKSUM':root/(archive_name+'.sha256'),
                'MANIFEST':root/(archive_name[:-4]+'.manifest.txt'),'NOTARY':root/(archive_name[:-4]+'.notary.json')},{},
               [app,receipt,helper.parent]+[Path(name) for name in accepted['inputs']],
               ['phase=final-artifact','staple_receipt='+str(receipt),'staple_receipt_sha256='+receipt_digest(receipt)]+[key+'='+value for key,value in selected.items()],
               command,list(tools.values()))


def artifact_input(repo, receipt):
    """Reproduce the final export lineage before selecting an app for refresh."""
    record=completed(repo,receipt);selected=values(record)
    if selected.get('phase')!='final-artifact':raise ValueError('Refresh requires completed final artifact')
    staple_receipt=Path(selected['staple_receipt'])
    if str(staple_receipt) not in record['source_identity']['inputs'] or receipt_digest(staple_receipt)!=selected['staple_receipt_sha256']:
        raise ValueError('Final artifact stapling receipt changed')
    app,accepted=stapled_input(repo,staple_receipt)
    identity={key:selected[key] for key in IDENTITY_FIELDS};identity_fields(identity);bundle_identity(app,identity)
    if record['source_identity']['inputs'].get(str(app))!=inventory(app):
        raise ValueError('Final artifact app input changed')
    outputs={item['name']:Path(record['output_root'])/item['relative'] for item in record['contract']['outputs']}
    if set(outputs)!={'ARCHIVE','CHECKSUM','MANIFEST','NOTARY','REPORTS'}:
        raise ValueError('Invalid final artifact output contract')
    plan(repo,Path(record['output_root']),[outputs['REPORTS']],[outputs[name] for name in ('ARCHIVE','CHECKSUM','MANIFEST','NOTARY')])
    zip_proof=verify_zip(outputs['ARCHIVE'],app)
    verify_sidecars(outputs['ARCHIVE'],outputs['CHECKSUM'],outputs['MANIFEST'],outputs['NOTARY'],identity,accepted,zip_proof)
    return app,identity,accepted


def refresh(repo, receipt, destination):
    """An already-authorized refresh consumes the exact completed final export."""
    repo=repo.resolve();receipt=Path(os.path.abspath(receipt));no_symlinks(receipt,repo)
    raw=read_json(receipt,16777216)
    root=Path(raw['output_root']);no_symlinks(root,repo)
    if receipt.parent.parent!=root/'.package-transactions':raise ValueError('Refresh artifact receipt root mismatch')
    descriptors=[]
    try:
        for path in (repo/'tmp/locks/clean.lock',root/'.package-transactions/owner.lock'):
            no_symlinks(path,repo);fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW);descriptors.append(fd)
            try:fcntl.flock(fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Final refresh held by active cleanup or artifact owner')
        app,identity,accepted=artifact_input(repo,receipt)
        result=replace_desktop(repo,app,destination,identity['bundle_id'])
        if artifact_input(repo,receipt)!=(app,identity,accepted):
            raise ValueError('Final artifact changed during refresh; replacement history retained')
        return result
    finally:
        for fd in descriptors:os.close(fd)


def text_assignments(items):
    selected={}
    for item in items:
        key,separator,value=item.partition('=')
        if not separator or key in selected:raise ValueError('Duplicate/invalid identity or tool assignment')
        selected[key]=value
    return selected


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','_assemble','refresh'))
    parser.add_argument('--staple-receipt',type=Path)
    parser.add_argument('--final-receipt',type=Path);parser.add_argument('--destination',type=Path)
    parser.add_argument('--root',type=Path);parser.add_argument('--archive-name',default='artifact.zip')
    parser.add_argument('--identity',action='append',default=[]);parser.add_argument('--tool',action='append',default=[])
    args,mappings=parser.parse_known_args()
    try:
        repo=Path.cwd().resolve()
        if args.action=='refresh':
            if (args.final_receipt is None or args.destination is None or args.staple_receipt or args.root
                    or args.identity or args.tool or mappings or args.archive_name!='artifact.zip'):
                raise ValueError('Refresh requires only final receipt and destination')
            print(json.dumps(refresh(repo,args.final_receipt,args.destination)));return
        if args.staple_receipt is None or args.final_receipt or args.destination:
            raise ValueError('Export requires only a stapling receipt')
        receipt=Path(os.path.abspath(args.staple_receipt))
        selected=text_assignments(args.identity);tools=text_assignments(args.tool)
        if args.action=='_assemble':
            if args.root:raise ValueError('Assembly root comes from transaction mappings')
            assemble(repo,receipt,selected,tools,parse_assignments(mappings))
        else:
            if args.root is None or mappings:raise ValueError('Final preparation requires selected root and no mappings')
            print(json.dumps(prepare(repo,receipt,args.root,selected,tools=tools or None,archive_name=args.archive_name)))
    except (ValueError,OSError,KeyError,TypeError) as error:
        parser.exit(2,'Final artifact held: '+str(error)+'\n')


if __name__=='__main__':main()
