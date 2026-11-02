"""Standalone SVG scientific plots of retained demonstrator observations.
No external plotting dependency; axes, native/reference styles and units explicit.
"""
from html import escape
import json
import numpy as np
COLORS={'warm':'#ba431e','off':'#697686','reverse':'#2166ac'}

def chart(parts,x,y,w,h,title,xlabel,ylabel,series,markers=(),xrange=None,yrange=None,heater=True):
    # series = (x values,y values,color,dashed,label)
    xmin,xmax=xrange or (min(float(np.min(s[0])) for s in series),max(float(np.max(s[0])) for s in series))
    ymin,ymax=yrange or (min(float(np.min(s[1])) for s in series),max(float(np.max(s[1])) for s in series))
    margin=(ymax-ymin)*.08 or 1
    if yrange is None:ymin-=margin;ymax+=margin
    left,top=x+73,y+42;pw,ph=w-97,h-103
    def X(v):return left+(v-xmin)/(xmax-xmin)*pw
    def Y(v):return top+ph-(v-ymin)/(ymax-ymin)*ph
    parts.append(f'<text x="{x+12}" y="{y+24}" font-size="17" font-weight="600">{escape(title)}</text>')
    if heater and xmin<=1<=xmax:
        parts.append(f'<rect x="{X(xmin)}" y="{top}" width="{X(1)-X(xmin)}" height="{ph}" fill="#faf1d8"/>')
    for j in range(6):
        vx=xmin+(xmax-xmin)*j/5;vy=ymin+(ymax-ymin)*j/5
        parts.append(f'<path d="M{X(vx):.2f} {top}v{ph} M{left} {Y(vy):.2f}h{pw}" stroke="#dde1e7"/>')
        parts.append(f'<text x="{X(vx):.2f}" y="{top+ph+20}" text-anchor="middle" font-size="12">{vx:.3g}</text><text x="{left-8}" y="{Y(vy)+4:.2f}" text-anchor="end" font-size="12">{vy:.4g}</text>')
    parts.append(f'<path d="M{left} {top}v{ph}h{pw}" stroke="#28323f" fill="none"/>')
    if heater and xmin<=1<=xmax:parts.append(f'<path d="M{X(1):.2f} {top}v{ph}" stroke="#9c7527" stroke-dasharray="4 4"/>')
    for tx,label in markers:
        parts.append(f'<path d="M{X(tx):.2f} {top}v{ph}" stroke="#8957a8" stroke-dasharray="2 4"/><text x="{X(tx)+4:.2f}" y="{top+13}" font-size="10" fill="#74498e">{escape(label)}</text>')
    for xs,ys,color,dashed,label in series:
        points=' '.join(f'{X(float(a)):.2f},{Y(float(b)):.2f}' for a,b in zip(xs,ys))
        dash=' stroke-dasharray="5 4"' if dashed else ''
        parts.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="{1.3 if dashed else 2}"{dash}><title>{escape(label)}</title></polyline>')
    parts.append(f'<text x="{left+pw/2}" y="{y+h-13}" text-anchor="middle" font-size="13">{escape(xlabel)}</text><text transform="translate({x+17},{top+ph/2}) rotate(-90)" text-anchor="middle" font-size="13">{escape(ylabel)}</text>')

def plots(root,data,report):
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="900" viewBox="0 0 1200 900"><rect width="1200" height="900" fill="white"/><g font-family="Arial,sans-serif" fill="#243140">',
      '<text x="25" y="32" font-size="24" font-weight="600">Five-second gas / solid demonstrator</text>',
      '<text x="25" y="57" font-size="14">Solid lines: native. Dashed: independent refined reference. Shading: heater on, 0–1 s.</text>',
      '<text x="25" y="80" font-size="14"><tspan fill="#ba431e">Warm gas / cool solid</tspan><tspan dx="35" fill="#697686">Exchange off</tspan><tspan dx="35" fill="#2166ac">Reversal</tspan></text>']
    for panel,(key,title,unit) in enumerate([('gas_mean_k','Gas mean temperature','Temperature (K)'),('rate','Signed wall heat-transfer rate','Power into solid (W)'),('q_j','Cumulative heat into solid','Signed heat (J)'),('solid','Solid response (rise above initial)','Temperature change (K)')]):
        series=[]
        for name,(native,ref) in data.items():
            for d,dashed in [(native,False),(ref,True)]:
                xs=d['time_s'];color=COLORS[name]
                if key=='rate':series.append(((xs[1:]+xs[:-1])/2,np.diff(d['q_j'])/.01,color,dashed,name))
                elif key=='solid':
                    series.append((xs,d['solid0_k']-d['solid0_k'][0],color,dashed,name+' front'))
                    back={'warm':'#d99425','off':'#697686','reverse':'#00a6a6'}[name]
                    series.append((xs,d['solid1_k']-d['solid1_k'][0],back,dashed,name+' back'))
                else:series.append((xs,d[key]+(300 if key=='gas_mean_k' else 0),color,dashed,name))
        markers=[]
        if key=='solid':
            for label,event in report['cases']['warm']['crossings'].items():
                if event['native_s'] is not None:markers.append((event['native_s'],f'{"Front" if label=="solid0_k" else "Back"} {event["native_s"]:.3f}s'))
        chart(parts,(panel%2)*600,95+(panel//2)*390,600,385,title,'Time (s)',unit,series,markers)
    parts.append('<text x="25" y="889" font-size="12">Solid panel: warm front red / back orange; reversal front blue / back cyan. Local startup gas points still fail 10%.</text></g></svg>')
    (root/'temperatures-and-heat.svg').write_text(''.join(parts))
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="475" viewBox="0 0 1200 475"><rect width="1200" height="475" fill="white"/><g font-family="Arial,sans-serif" fill="#243140"><text x="25" y="30" font-size="22">Wall-normal gas temperatures: matched cell-average supports</text><text x="25" y="54" font-size="14">Solid: native 256 cells. Dashed: 2048-cell independent reference averaged onto the same cells.</text>']
    palette=['#d99425','#ba431e','#6a55a1','#2166ac','#178465']
    for panel,name in enumerate(['warm','reverse']):
        series=[]
        for index,color in zip([10,50,100,200,500],palette):
            for d,dashed in [(data[name][0],False),(data[name][1],True)]:series.append(((np.arange(256)+.5)*.0625/256*1000,d['profile_k'][index]+300,color,dashed,f'{index/100:g}s'))
        chart(parts,panel*600,70,600,375,name.capitalize()+' — 0.1, 0.5, 1, 2, 5 s','Distance from wall (mm)','Gas temperature (K)',series,heater=False)
    parts.append('<text x="25" y="463" font-size="12"><tspan fill="#d99425">0.1 s</tspan><tspan dx="45" fill="#ba431e">0.5 s</tspan><tspan dx="45" fill="#6a55a1">1 s</tspan><tspan dx="45" fill="#2166ac">2 s</tspan><tspan dx="45" fill="#178465">5 s</tspan></text></g></svg>')
    (root/'normal-profiles.svg').write_text(''.join(parts))

    # Zoom the remaining failure rather than allowing a five-second overview
    # to hide the first 20 ms. Same cell-average support for all four curves.
    def short(name):
        return [json.loads(s) for s in (root/'front-check'/name).read_text().splitlines()]
    fine=short('8192-409600.jsonl');same=short('256-102400.jsonl');half=short('512-102400.jsonl')
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="475" viewBox="0 0 1200 475"><rect width="1200" height="475" fill="white"/><g font-family="Arial,sans-serif" fill="#243140"><text x="25" y="30" font-size="22">Reversal startup: the remaining error is already in the gas field</text><text x="25" y="54" font-size="14">Red: native. Purple: same 256-cell CN. Blue: 512-cell CN. Dashed green: qualified fine reference.</text>']
    z=(np.arange(256)+.5)*.0625/256*1000;mask=(z>=1)&(z<=2.5)
    for panel,index in enumerate([1,2]):
        series=[]
        for values,color,dashed,label in [(data['reverse'][0]['profile_k'][index],'#ba431e',False,'Native256'),(same[index]['profile_k'],'#8957a8',False,'CN256'),(half[index]['profile_k'],'#2166ac',False,'CN512'),(fine[index]['profile_k'],'#178465',True,'Fine reference')]:
            series.append((z[mask],np.asarray(values)[mask],color,dashed,label))
        chart(parts,panel*600,70,600,375,f'At {index/100:g} s, matched normal cells','Distance from wall (mm)','Gas rise above 300 K (K)',series,heater=False)
    parts.append('<text x="25" y="463" font-size="12">At 10 ms, z=1.587 mm: native +25.32%; same-grid CN +13.42%; 512-cell CN +3.48%. No startup gate waived.</text></g></svg>')
    (root/'startup-front.svg').write_text(''.join(parts))
