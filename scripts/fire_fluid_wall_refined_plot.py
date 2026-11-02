"""Export retained solver-profile results as standalone SVG scientific plots."""
from pathlib import Path
import json
import numpy as np
from fire_fluid_warm_solid_plot import chart
from fire_fluid_wall_refined import rows,arrays

def plots(root):
    data={name:arrays(rows(root/'runs'/f'{name}-probe.jsonl')[:-1]) for name in ['warm','off','reverse']}
    colors={'warm':'#ba431e','off':'#697686','reverse':'#2166ac'}
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="900" viewBox="0 0 1200 900"><rect width="1200" height="900" fill="white"/><g font-family="Arial,sans-serif" fill="#243140"><text x="25" y="32" font-size="23" font-weight="600">Implemented wall-refined solver: five-second gas / solid response</text><text x="25" y="57" font-size="14">512 normal cells; conservative CN diffusion; centered one-step wall exchange; original scalar timestep.</text><text x="25" y="80" font-size="14"><tspan fill="#ba431e">Warm gas / cool solid</tspan><tspan dx="35" fill="#697686">Exchange off</tspan><tspan dx="35" fill="#2166ac">Reversal</tspan></text>']
    for panel,(key,title,unit) in enumerate([('gas_mean_k','Gas mean temperature','Temperature (K)'),('rate','Signed wall transfer rate','Power into solid (W)'),('q_j','Cumulative heat into solid','Signed heat (J)'),('solid','Solid response above initial','Temperature change (K)')]):
        series=[]
        for name,d in data.items():
            x=d['time_s'];color=colors[name]
            if key=='rate':series.append(((x[1:]+x[:-1])/2,np.diff(d['q_j'])/.01,color,False,name))
            elif key=='solid':
                series.append((x,d['solid0_k']-d['solid0_k'][0],color,False,name+' front'))
                back={'warm':'#d99425','off':'#697686','reverse':'#00a6a6'}[name];series.append((x,d['solid1_k']-d['solid1_k'][0],back,False,name+' back'))
            else:series.append((x,d[key]+(300 if key=='gas_mean_k' else 0),color,False,name))
        chart(parts,panel%2*600,95+panel//2*390,600,385,title,'Time (s)',unit,series)
    parts.append('<text x="25" y="889" font-size="12">Shading: heater on 0–1 s. Solid front/back: red/orange warm; blue/cyan reversal. Authored constant-property two-cell solid model.</text></g></svg>');(root/'solver-response.svg').write_text(''.join(parts))
    fine=arrays(rows(root.parent/'fire-fluid-reversal-timing-20261020/runs/reference-8192-409600.jsonl'));old=arrays(rows(root.parent/'fire-fluid-reversal-timing-20261020/runs/s1-e5-probe.jsonl')[:-1]);fast=arrays(rows(root/'runs/short-256-probe.jsonl')[:-1]);new=arrays(rows(root/'runs/short-512-probe.jsonl')[:-1])
    z=(np.arange(256)+.5)*.0625/256*1000;mask=(z>=1)&(z<=2.5)
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="480" viewBox="0 0 1200 480"><rect width="1200" height="480" fill="white"/><g font-family="Arial,sans-serif" fill="#243140"><text x="25" y="30" font-size="23">The solver change reduces early gas-front error</text><text x="25" y="55" font-size="14"><tspan fill="#ba431e">Original 256 / 5-step exchange</tspan><tspan dx="25" fill="#d99425">256 / 1-step exchange</tspan><tspan dx="25" fill="#178465">New wall-refined solver</tspan><tspan dx="25" fill="#697686">Fine reference (dashed)</tspan></text>']
    for panel,index in enumerate([1,2]):
        series=[(z[mask],d['profile_k'][index][mask],color,dashed,label) for d,color,dashed,label in [(old,'#ba431e',False,'Original'),(fast,'#d99425',False,'Faster exchange only'),(new,'#178465',False,'Wall-refined'),(fine,'#697686',True,'Fine reference')]]
        chart(parts,panel*600,70,600,375,f'At {index/100:g} s, identical physical supports','Distance from wall (mm)','Gas rise above 300 K (K)',series,heater=False)
    parts.append('<text x="25" y="465" font-size="12">Worst startup error: original 25.32%; faster exchange 16.50%; new solver 3.67%. The 10% gate is unchanged; no heat floor.</text></g></svg>');(root/'solver-front.svg').write_text(''.join(parts))
if __name__=='__main__':
    import sys
    plots(Path(sys.argv[1]).resolve())
