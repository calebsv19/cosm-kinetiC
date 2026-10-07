"""Native sparse/dense parity and independent rejection controls."""
import copy
import math
import struct
import sys
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts')); sys.path.insert(0,str(ROOT/'tests'))
from test_open_atmosphere import request, WORKER
from open_atmosphere import run, validate
from numeric_digest_stream import digest
from plume_qualification import write_schedule, run_sparse, read_sample, SAMPLE

class QualificationTests(unittest.TestCase):
    def scenario(self):
        r=request(8,.01); n=512
        r['transport_scheme']='muscl_minmod'; r['buoyancy']['enabled']=True
        r['boundary_policy'].update(vertical='solid_bottom_open_top',
                                   predictor_velocity='no_slip_bottom_zero_gradient_top')
        r['resource_limits']={'max_cells':32768,'scalar_work_cells':1000000000,
                             'cache_pressure_operator':True}
        r['steps']=[]; sparse=[]
        for step in range(20):
            energy=[0.]*n; smoke=[0.]*n; cells=[]
            for index in (0,1,9,67):
                energy[index]=.001*(step+1); smoke[index]=1e-8*(step+1)
                cells.append({'cell_index':index,'energy_transferred_j':energy[index],
                              'smoke_transferred_kg':smoke[index]})
            r['steps'].append({'energy_j':energy,'smoke_kg':smoke}); sparse.append(cells)
        return r,sparse

    def test_exact_dense_parity_both_sample_times(self):
        r,sparse=self.scenario()
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); schedule=root/'forcing.bin'
            totals=write_schedule(schedule,r,sparse,[10,20])
            receipt=run_sparse(r,WORKER,schedule,root/'native',timeout_s=30)
            for count in (10,20):
                decoded=read_sample(root/f'native/sample-{count:04}.bin',r,
                    receipt['worker_sha256'],totals[count-1])
                dense=run(dict(r,steps=r['steps'][:count]),WORKER)
                self.assertEqual(decoded['state'],dense['state'])
                self.assertEqual(digest({'data':decoded['state']['data']}),
                                 digest({'data':dense['state']['data']}))
                self.assertEqual(decoded['budgets'],dense['budgets'])
            self.assertGreater(receipt['peak_temperature_k'],300)
            self.assertGreater(receipt['peak_velocity_component_m_s'],0)

    def test_malformed_forcing_and_sample_rejected(self):
        r,sparse=self.scenario()
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); schedule=root/'forcing.bin'
            totals=write_schedule(schedule,r,sparse,[20]); original=schedule.read_bytes()
            for index,blob in enumerate((original[:-1],original+b'x',b'BADMAGIC'+original[8:])):
                bad=root/f'bad-{index}.bin';bad.write_bytes(blob)
                with self.assertRaises(ValueError):run_sparse(r,WORKER,bad,root/f'native-{index}',timeout_s=30)
            run_sparse(r,WORKER,schedule,root/'valid',timeout_s=30)
            sample=root/'valid/sample-0020.bin'; bad=root/'sample.bin';bad.write_bytes(sample.read_bytes()[:-1])
            with self.assertRaises(ValueError):read_sample(bad,r,'worker',totals[-1])
            # The packet reader must run the ordinary field/source gates, rather
            # than trust a successful worker exit or a correctly sized packet.
            original_sample=sample.read_bytes()
            for index,(offset,value) in enumerate(((SAMPLE.size,float('nan')),
                    (SAMPLE.size+8*(5*512+64),-1.),(32,.3))):
                blob=bytearray(original_sample);struct.pack_into('<d',blob,offset,value)
                corrupt=root/f'corrupt-{index}.bin';corrupt.write_bytes(blob)
                with self.assertRaises(ValueError):read_sample(corrupt,r,'worker',totals[-1])
            with self.assertRaises(ValueError):read_sample(sample,r,'worker',
                {'energy_j':totals[-1]['energy_j']*2,'smoke_kg':totals[-1]['smoke_kg']})

    def test_preflight_rejects_duplicate_negative_and_time_bound(self):
        r,sparse=self.scenario()
        with tempfile.TemporaryDirectory() as temp:
            for index,rows in enumerate((sparse[:1]+[sparse[0]*2],
                    [[{'cell_index':0,'energy_transferred_j':-1,'smoke_transferred_kg':0}]])):
                with self.assertRaises(ValueError):write_schedule(Path(temp)/f'bad-{index}',r,rows,[len(rows)])
            wrong=copy.deepcopy(r);wrong['momentum_dt_s']=.1
            with self.assertRaises(ValueError):write_schedule(Path(temp)/'duration',wrong,[[]]*81,[81])

    def test_explicit_work_ceiling_and_ordinary_defaults(self):
        r,_=self.scenario();r['steps']=[]
        validate(r)
        r['resource_limits']['scalar_work_cells']=1000000001
        with self.assertRaises(ValueError):validate(r)
        # Removing experimental resource controls retains the ordinary request.
        del r['resource_limits'];validate(r)

    def test_movie_samples_exact_and_qualification_caps_preserved(self):
        r,sparse=self.scenario();r['resource_limits']['scalar_work_cells']=2000000000
        with self.assertRaises(ValueError):validate(r)
        validate(r,movie=True)
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);schedule=root/'movie.bin'
            totals=write_schedule(schedule,r,sparse,[5,10,15,20],movie=True)
            receipt=run_sparse(r,WORKER,schedule,root/'movie',timeout_s=30,movie=True)
            self.assertEqual(receipt['schema'],'physics_sim_sparse_movie_receipt/v1')
            ordinary=copy.deepcopy(r);ordinary['resource_limits']['scalar_work_cells']=1000000000
            for count in (5,10,15,20):
                movie=read_sample(root/f'movie/sample-{count:04}.bin',r,
                    receipt['worker_sha256'],totals[count-1],movie=True)
                dense=run(dict(ordinary,steps=ordinary['steps'][:count]),WORKER)
                self.assertEqual(movie['state']['data'],dense['state']['data'])
                self.assertEqual(movie['budgets'],dense['budgets'])
                compact=read_sample(root/f'movie/sample-{count:04}.bin',r,
                    receipt['worker_sha256'],totals[count-1],movie=True,compact=True)
                self.assertEqual(compact['schema'],'physics_sim_movie_sample_fields/v1')
                self.assertEqual(compact['native_data'],dense['state']['data'])
                self.assertEqual(compact['budgets'],dense['budgets'])
                self.assertEqual(compact['fields'],movie['fields'])
                self.assertNotIn('digest',compact)
                self.assertFalse(compact['native_checkpoint_restart'])
            # Selection of movie mode is mandatory even for a short packet.
            with self.assertRaises(ValueError):
                run_sparse(ordinary,WORKER,schedule,root/'ordinary',timeout_s=30)
            with self.assertRaises(ValueError):
                write_schedule(root/'too-long',r,[[]]*6401,[6401],movie=True)
            with self.assertRaises(ValueError):
                write_schedule(root/'too-many',r,[[]]*161,list(range(1,162)),movie=True)
            wrong={'energy_j':totals[-1]['energy_j']*2,'smoke_kg':totals[-1]['smoke_kg']}
            with self.assertRaises(ValueError):read_sample(root/'movie/sample-0020.bin',r,
                receipt['worker_sha256'],wrong,movie=True,compact=True)

    def test_movie_extended_time_and_native_bound_rejection(self):
        r=request(8,.1);r['steps']=[]
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);schedule=root/'nine-seconds.bin'
            totals=write_schedule(schedule,r,[[]]*90,[20,40,60,90],movie=True)
            receipt=run_sparse(r,WORKER,schedule,root/'movie',timeout_s=7201,movie=True)
            # Wall allowance extends only explicitly selected movie mode.
            for mode,limit,label in ((False,7201,'ordinary-wall'),(True,14401,'movie-wall')):
                with self.assertRaises(ValueError):
                    run_sparse(r,WORKER,schedule,root/label,timeout_s=limit,movie=mode)
                self.assertFalse((root/label).exists())
            result=read_sample(root/'movie/sample-0090.bin',r,receipt['worker_sha256'],totals[-1],movie=True)
            self.assertEqual(result['fields']['time_s'],9)
            self.assertEqual(result['state']['data']['steps'],90)
            with self.assertRaises(ValueError):run_sparse(r,WORKER,schedule,root/'ordinary',timeout_s=30)
            with self.assertRaises(ValueError):write_schedule(root/'duration',r,[[]]*321,[321],movie=True)

if __name__=='__main__':unittest.main()
