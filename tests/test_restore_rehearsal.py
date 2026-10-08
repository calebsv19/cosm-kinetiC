"""Restore rehearsals refuse overwrites, unsafe archives and checksum drift."""
import io
import json
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from restore_rehearsal import extract,rehearse,PAYLOAD
from cfd_evidence import sha,seal_bundle


class Restore(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve()

    def archive(self,name,entries):
        archive=self.root/name
        with tarfile.open(archive,'w:gz') as stream:
            for path,content,kind in entries:
                member=tarfile.TarInfo(path);member.type=kind;member.mode=0o644
                if kind==tarfile.REGTYPE:member.size=len(content);stream.addfile(member,io.BytesIO(content))
                else:member.linkname='outside';stream.addfile(member)
        return archive

    def test_regular_restore_reads_back_and_refuses_existing_destination(self):
        archive=self.archive('safe.tar.gz',[('build/probe',b'preserved',tarfile.REGTYPE)])
        destination=self.root/'restore';row=extract(archive,destination)
        self.assertEqual(row['files'],1);self.assertEqual((destination/'build/probe').read_bytes(),b'preserved')
        with self.assertRaises(FileExistsError):extract(archive,destination)
        self.assertEqual((destination/'build/probe').read_bytes(),b'preserved')

    def test_escape_links_special_and_duplicate_members_are_held(self):
        for index,entries in enumerate((
            [('../escape',b'bad',tarfile.REGTYPE)],
            [('/absolute',b'bad',tarfile.REGTYPE)],
            [('link',b'',tarfile.SYMTYPE)],
            [('link',b'',tarfile.LNKTYPE)],
            [('pipe',b'',tarfile.FIFOTYPE)],
            [('same',b'first',tarfile.REGTYPE),('same',b'second',tarfile.REGTYPE)])):
            archive=self.archive(str(index)+'.tar.gz',entries)
            with self.assertRaises(ValueError):extract(archive,self.root/('restore-'+str(index)))
        self.assertFalse((self.root/'escape').exists())

    def test_resource_limit_preserves_partial_identity_and_never_reuses_it(self):
        archive=self.archive('large.tar.gz',[('large',b'content',tarfile.REGTYPE)])
        destination=self.root/'partial'
        with self.assertRaises(ValueError):extract(archive,destination,max_bytes=3)
        self.assertTrue(destination.exists())
        with self.assertRaises(FileExistsError):extract(archive,destination)

    def payload(self):
        payload=self.root/'payload';payload.mkdir()
        historical=self.archive('history.tar.gz',[('build/probe',b'historical',tarfile.REGTYPE)])
        historical.rename(payload/'historical-survivors.tar.gz')
        source=self.root/'fresh/data/experiments/proof';source.mkdir(parents=True)
        (source/'input').write_bytes(b'evidence');seal_bundle(source)
        with tarfile.open(payload/'fresh-lifecycle-evidence.tar.gz','w:gz') as stream:
            stream.add(self.root/'fresh/data',arcname='data')
        (payload/'survivor-manifest.json').write_text(json.dumps([{'path':'build/probe','bytes':10,
            'sha256':__import__('hashlib').sha256(b'historical').hexdigest()}]))
        for name in ('RESTORE.md','incident.md','repair-status.md'):(payload/name).write_text('retained note\n')
        receipt=self.root/'copy.json';receipt.write_text(json.dumps({'status':'verified_independent_cold_archive_copy',
            'archive_destination':'exact approved archive','payload_checksums':{name:sha(payload/name) for name in PAYLOAD}}))
        return payload,receipt

    def test_checksum_bound_historical_and_sealed_fresh_readback(self):
        payload,receipt=self.payload();row=rehearse(payload,receipt,self.root/'restored')
        self.assertTrue(row['historical_manifest_matched']);self.assertEqual(row['historical_restored_files'],1)
        self.assertEqual(len(row['fresh_bundles_verified']),1);self.assertFalse(row['later_evidence_covered'])
        self.assertFalse(row['runtime_or_numerical_qualification_verified'])

    def test_checksum_drift_and_payload_overlap_refused_before_restore(self):
        payload,receipt=self.payload()
        with self.assertRaises(ValueError):rehearse(payload,receipt,payload/'restore')
        (payload/'incident.md').write_text('changed')
        destination=self.root/'restored'
        with self.assertRaises(ValueError):rehearse(payload,receipt,destination)
        self.assertFalse(destination.exists())


if __name__=='__main__':unittest.main()
