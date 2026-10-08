"""Cross-language discovery is bounded, read-only and never behavior acceptance."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import writer_inventory as w

class Inventory(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.repo=Path(self.tmp.name).resolve()
        for name in ('scripts','src','include','make','tests'):(self.repo/name).mkdir()
    def put(self,name,text):p=self.repo/name;p.write_text(text);return p
    def snapshot(self):return {str(p.relative_to(self.repo)):(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in self.repo.rglob('*') if p.is_file() and not p.is_symlink()}
    def test_python_modes_and_adapters_are_signals_without_execution(self):
        self.put('scripts/tool.py',"from pathlib import Path\nPath('never-created').write_text('x')\nopen('reader','rb')\nopen('writer','w')\nopen('dynamic',mode)\nreference_ownership(repo,data)\n")
        before=self.snapshot();report=w.inventory(self.repo);row=report['files']['scripts/tool.py']
        self.assertEqual({x['kind'] for x in row['signals']},{'write_or_publish','write_open','dynamic_open_mode'})
        self.assertIn('reference_ownership',row['observed_adapter_calls']);self.assertFalse(row['behavior_coverage_verified']);self.assertEqual(self.snapshot(),before)
        self.assertFalse((self.repo/'never-created').exists());self.assertTrue(report['discovery_complete_for_selected_scan'])
    def test_native_shell_and_make_calls_have_lines_and_recipe_edges(self):
        self.put('src/writer.c','// fopen(path,"w");\nFILE *f=fopen(path,"wb");\nFILE *r=fopen(path,"rb");\nrename(tmp,final);\n')
        self.put('include/writer.h','int native_save(const char *path);\n')
        self.put('scripts/tool.sh','#!/bin/sh\nrm -rf "$ROOT/output"\nmkdir -p "$ROOT/new"\npython3 scripts/tool.py > "$ROOT/report"\n')
        self.put('Makefile','go:\n\tpython3 scripts/tool.py > report.json\n')
        report=w.inventory(self.repo);native=report['files']['src/writer.c']['signals']
        self.assertEqual([(x['line'],x['operation']) for x in native],[(2,'fopen'),(4,'rename')])
        self.assertTrue(report['files']['include/writer.h']['signals'])
        self.assertTrue(any(x['kind']=='delete' for x in report['files']['scripts/tool.sh']['signals']))
        self.assertEqual(report['files']['Makefile']['explicit_script_edges'],[{'line':2,'target':'go','script':'scripts/tool.py'}])
    def test_links_special_files_and_parse_errors_are_explicit_gaps(self):
        outside=self.repo/'outside';outside.write_text('sentinel');(self.repo/'scripts/link.py').symlink_to(outside);os.mkfifo(self.repo/'scripts/fifo.py')
        self.put('scripts/broken.py','def broken(:')
        report=w.inventory(self.repo);self.assertFalse(report['discovery_complete_for_selected_scan']);self.assertEqual(len(report['gaps']),3);self.assertEqual(outside.read_text(),'sentinel')
    def test_file_aggregate_and_entry_bounds_hold_without_source_mutation(self):
        self.put('scripts/a.py','x=1\n');self.put('scripts/b.py','y=2\n');before=self.snapshot()
        for options in ({'file_limit':2},{'total_limit':5},{'entry_limit':3}):
            report=w.inventory(self.repo,**options);self.assertFalse(report['discovery_complete_for_selected_scan']);self.assertTrue(report['gaps'])
        self.assertEqual(self.snapshot(),before)
    def test_cli_never_writes_inventory_or_executes_source(self):
        self.put('scripts/tool.py',"open('never-created','w').write('x')\n");before=self.snapshot()
        result=subprocess.run([sys.executable,'-B',str(ROOT/'scripts/writer_inventory.py'),'--repo',str(self.repo)],cwd=self.repo,capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);report=json.loads(result.stdout);self.assertFalse(report['behavior_coverage_verified']);self.assertFalse(report['deletion_authorized']);self.assertEqual(self.snapshot(),before)

    def test_packaging_and_extensionless_entrypoints_are_discovered_without_execution(self):
        (self.repo/'tools/packaging/macos').mkdir(parents=True)
        self.put('tools/packaging/macos/physics-sim-launcher', '#!/bin/sh\nmkdir -p "$RUNTIME"\n')
        self.put('tools/packaging/package.py', "open('never-created','w').write('x')\n")
        self.put('tools/packaging/opaque', '\x00binary')
        self.put('rm_frames', '#!/usr/bin/env bash\nrm -f export/render_frames/frame_*.bmp\n')
        self.put('Makefile', 'package:\n\tsh tools/packaging/macos/physics-sim-launcher\n')
        before=self.snapshot();report=w.inventory(self.repo)
        self.assertEqual(report['files']['tools/packaging/macos/physics-sim-launcher']['language'],'shell')
        self.assertTrue(report['files']['tools/packaging/package.py']['signals'])
        self.assertEqual(report['files']['rm_frames']['signals'][0]['kind'],'delete')
        self.assertNotIn('tools/packaging/opaque',report['files'])
        self.assertEqual(report['files']['Makefile']['explicit_script_edges'][0]['script'],'tools/packaging/macos/physics-sim-launcher')
        self.assertEqual(self.snapshot(),before);self.assertTrue(report['discovery_complete_for_selected_scan'])
        self.assertFalse((self.repo/'never-created').exists())

    def test_extensionless_python_and_unsupported_interpreter_have_explicit_scope(self):
        (self.repo/'tools').mkdir()
        self.put('tools/writer', "#!/usr/bin/env python3\nopen('never-created','w')\n")
        self.put('tools/unsupported', '#!/usr/bin/perl\nprint "x";\n')
        report=w.inventory(self.repo)
        self.assertEqual(report['files']['tools/writer']['language'],'python')
        self.assertEqual(len(report['gaps']),1);self.assertIn('unsupported',report['gaps'][0]['reason'])
        self.assertFalse(report['discovery_complete_for_selected_scan'])

    def test_extensionless_sources_obey_file_bound_and_refuse_links(self):
        (self.repo/'tools').mkdir();self.put('tools/large','#!/bin/sh\n'+'x'*500)
        (self.repo/'tools/link').symlink_to(self.repo/'tools/large')
        before=self.snapshot();report=w.inventory(self.repo,file_limit=100)
        self.assertEqual(len(report['gaps']),2);self.assertFalse(report['discovery_complete_for_selected_scan'])
        self.assertEqual(self.snapshot(),before)

    def test_make_packaging_variable_bindings_are_discovery_edges(self):
        self.put('Makefile', 'PACKAGE_BUNDLER ?= $(CURDIR)/tools/packaging/macos/bundle-dylibs.sh\npackage:\n\t"$(PACKAGE_BUNDLER)" "$BIN" "$FRAMEWORKS"\n')
        report=w.inventory(self.repo)
        self.assertEqual(report['files']['Makefile']['explicit_script_edges'], [{'line':1,'target':None,'script':'tools/packaging/macos/bundle-dylibs.sh','variable':'PACKAGE_BUNDLER'}])

if __name__=='__main__':unittest.main()
