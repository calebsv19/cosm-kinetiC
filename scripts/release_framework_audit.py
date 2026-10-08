"""Write distinct retained dependency reports for every regular framework dylib."""
import argparse
import json
from pathlib import Path
import re
import subprocess

from desktop_replace import inventory
from package_outputs import plan, declare


def verify_report(path, report):
    if report.stat().st_size>1048576:raise ValueError('Dependency report exceeds accepted size')
    with report.open('rb') as stream:payload=stream.read(1048577)
    if len(payload)>1048576:raise ValueError('Dependency report exceeds accepted size')
    header=re.compile(re.escape(str(path))+r'(?: \(architecture [A-Za-z0-9_-]+\))?:')
    dependencies='\n'.join(line for line in payload.decode().splitlines() if not header.fullmatch(line))
    if re.search(r'/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork',dependencies):
        raise ValueError('Non-portable dylib dependency detected in '+str(path))


def run(frameworks, reports, tool):
    admission=plan(Path.cwd(),reports,[],[reports/'index.json'])
    if not admission['fresh']:raise ValueError('Existing framework index retained')
    rows=inventory(frameworks)
    selected=[name for name,row in sorted(rows.items()) if row['kind']=='file' and name.endswith('.dylib')]
    files=[reports/('otool_'+str(n+1)+'.txt') for n in range(len(selected))]+[reports/'index.json']
    validate=lambda:plan(Path.cwd(),reports,[],files)
    declare(validate(),validate)
    mapping=[]
    for name in selected:
        path=frameworks/name;report=reports/('otool_'+str(len(mapping)+1)+'.txt')
        with report.open('xb') as stream:
            subprocess.run([tool,'-L',str(path)],stdout=stream,check=True,timeout=30)
        verify_report(path,report)
        mapping.append({'framework':name,'report':report.name})
    if inventory(frameworks)!=rows:raise ValueError('Framework inputs changed during audit')
    with (reports/'index.json').open('x') as stream:json.dump(mapping,stream,indent=2);stream.write('\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frameworks',type=Path);parser.add_argument('--reports',type=Path)
    parser.add_argument('--otool');parser.add_argument('--verify-report',type=Path);parser.add_argument('--input-binary',type=Path);args=parser.parse_args()
    if args.verify_report:
        if not args.input_binary or any((args.frameworks,args.reports,args.otool)):parser.error('Report verification requires only report and input binary')
    elif not all((args.frameworks,args.reports,args.otool)) or args.input_binary:parser.error('Framework audit requires frameworks, reports and otool')
    try:
        if args.verify_report:verify_report(args.input_binary,args.verify_report)
        else:run(args.frameworks,args.reports,args.otool)
    except (ValueError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as error:parser.exit(2,str(error)+'\n')

if __name__=='__main__':main()
