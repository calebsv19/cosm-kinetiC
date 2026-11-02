"""Independent admission/ledger tests; solver integration remains separate."""
import copy,json,math,os
from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.open_weather import validate,command,state,observation,SCHEMA,STATE
from coupling.overnight_cases import configuration
from coupling.kinetic_energy import kinetic

class OpenWeatherTests(unittest.TestCase):
    def config(self):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text())
        c=configuration((w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')]),'gust16')['fluid']
        c.pop('wind_forcing')
        c['open_domain_forcing']={'schema':SCHEMA,'ramp_height_m':.25,'response_s':.1,
            'interpolation':'hold','momentum_backflow':'quiescent_exterior/v1',
            'knots':[{'step':0,'target_m_s':.5,'strength':1},
                     {'step':40,'target_m_s':.5,'strength':0}]}
        return c
    def data(self,c):
        return {'steps':20,'face_velocity_m_s':c['initial_face_velocity_m_s'][:],
                'kinetic_ledger_j':[0.]*10,'kinetic_stored_j':0.,
                'open_domain_forcing_state':{'schema':STATE,'external_work_j':0.,'air_in_m3':0.,'air_out_m3':0.}}
    def test_release_is_not_zero_target(self):
        c=self.config();validate(c)
        self.assertEqual(command(c,39),(.5,1));self.assertEqual(command(c,40),(.5,0))
    def test_linear_reversal_and_fade(self):
        c=self.config();w=c['open_domain_forcing'];w['interpolation']='linear'
        w['knots'][-1]['target_m_s']=-.5
        self.assertEqual(command(c,20),(0.,.5));self.assertEqual(command(c,50),(-.5,0.))
    def test_conflicting_modes_and_bad_policies_refuse(self):
        for key,value in [('wind_forcing',{}),('diagnostic_transport','fixed_mac_velocity/v1')]:
            c=self.config();c[key]=value
            with self.assertRaises(ValueError):validate(c)
        for key,value in [('response_s',0),('momentum_backflow','legacy'),('ramp_height_m',2)]:
            c=self.config();c['open_domain_forcing'][key]=value
            with self.assertRaises(ValueError):validate(c)
    def test_external_work_is_separate_and_signed(self):
        c=self.config();d=self.data(c);d['kinetic_ledger_j'][5]=.1
        d['open_domain_forcing_state']['external_work_j']=-.1
        state(c,d)
        d['open_domain_forcing_state']['external_work_j']=0
        with self.assertRaises(ValueError):state(c,d)
    def test_inlet_work_and_air_defect_refuse(self):
        c=self.config();d=self.data(c);d['kinetic_ledger_j'][6]=.1
        d['open_domain_forcing_state']['external_work_j']=-.1
        with self.assertRaises(ValueError):state(c,d)
        d=self.data(c);d['open_domain_forcing_state']['air_in_m3']=.01
        with self.assertRaises(ValueError):state(c,d)
    def test_observation_identity_and_initial_state(self):
        c=self.config();d=self.data(c);d['steps']=0;state(c,d)
        fields=copy.deepcopy(d);self.assertEqual(observation(c,fields,d,None)['schema'],'physics_sim_open_weather_observation/v1')
        fields['open_domain_forcing_state']['external_work_j']=1
        with self.assertRaises(ValueError):observation(c,fields,d,None)
        d['open_domain_forcing_state']['external_work_j']=1
        with self.assertRaises(ValueError):state(c,d)
    def test_nonzero_mac_work_and_stored_energy(self):
        c=self.config();d=self.data(c)
        d['face_velocity_m_s'][0]=.1
        d['kinetic_stored_j']=kinetic(c,d['face_velocity_m_s'])
        d['open_domain_forcing_state']['external_work_j']=d['kinetic_stored_j']
        state(c,d)
        d['kinetic_stored_j']+=1e-4
        with self.assertRaises(ValueError):state(c,d)
    def test_unbound_sidecar_refuses(self):
        c=self.config();d=self.data(c);c.pop('open_domain_forcing')
        with self.assertRaises(ValueError):state(c,d)

if __name__=='__main__':unittest.main()
