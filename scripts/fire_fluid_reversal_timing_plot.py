"""Standalone scientific plot of the bounded timing matrix and remaining front."""
import json
from pathlib import Path
import numpy as np
from fire_fluid_warm_solid_plot import chart

def plots(root):
    report=json.loads((root/'report.json').read_text())
    def rows(name):return [json.loads(s) for s in (root/'runs'/name).read_text().splitlines()]
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="520" viewBox="0 0 1200 520"><rect width="1200" height="520" fill="white"/><g font-family="Arial,sans-serif" fill="#243140">',
      '<text x="25" y="30" font-size="23" font-weight="600">Reversal startup at 256 normal cells: timing improves, spatial error remains</text>',
      '<text x="25" y="55" font-size="14">All cases stop at 20 ms. Initial state, source, receiver step, bounds, wall law and 10% gate stay fixed.</text>',
      '<text x="25" y="80" font-size="12"><tspan fill="#ba431e">Scalar dt 1/51200 s</tspan><tspan dx="22" fill="#2166ac">1/102400 s</tspan><tspan dx="22" fill="#178465">1/204800 s</tspan></text>',
      '<text x="620" y="80" font-size="12"><tspan fill="#ba431e">Native baseline</tspan><tspan dx="16" fill="#178465">Both refined</tspan><tspan dx="16" fill="#8957a8">CN256</tspan><tspan dx="16" fill="#697686">Fine ref.</tspan></text>']
    series=[]
    for f,color in [(1,'#ba431e'),(2,'#2166ac'),(4,'#178465')]:
        x=np.asarray([1,2,5])*1e6/6400
        y=np.asarray([report['cases'][f's{f}-e{e}']['worst']['relative_error']*100 for e in [1,2,5]])
        series.append((x,y,color,False,f'scalar factor {f}'))
    series.append(([156.25,781.25],[10,10],'#ad2831',True,'10% physical gate'))
    spatial=report['same_grid_physical_errors']['profile_k']['point']*100
    series.append(([156.25,781.25],[spatial,spatial],'#8957a8',True,'Worst CN256 spatial error'))
    chart(parts,0,92,600,390,'Worst observed gas point error','Nominal exchange window (microseconds)','Error vs fine reference (%)',series,yrange=(0,28),heater=False)
    z=(np.arange(256)+.5)*.0625/256*1000;mask=(z>=1)&(z<=2.5)
    series=[]
    for name,color,dashed,label in [('s1-e5-probe.jsonl','#ba431e',False,'Native baseline'),('s4-e1-probe.jsonl','#178465',False,'Both refined'),('reference-256-204800.jsonl','#8957a8',True,'CN256'),('reference-8192-409600.jsonl','#697686',True,'Fine reference')]:
        values=np.asarray(rows(name)[1]['profile_k']);series.append((z[mask],values[mask],color,dashed,label))
    chart(parts,600,92,600,390,'Matched normal temperature at 10 ms','Distance from wall (mm)','Gas rise above 300 K (K)',series,heater=False)
    parts.append('<text x="25" y="506" font-size="12">Worst point: baseline 25.32%; scalar-only 25.09%; exchange-only 16.50%; both 16.19%. Worst same-grid CN: 14.25%. All fail 10%.</text></g></svg>')
    (root/'timing-separation.svg').write_text(''.join(parts))
if __name__=='__main__':
    import sys
    plots(Path(sys.argv[1]).resolve())
