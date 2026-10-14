#!/usr/bin/env python3
"""Standalone accepted-run review: SI observables, native field maps and authoring provenance.

Run with the bundled document Python runtime (Pillow/ReportLab); no simulation or
existing authored scene is mutated. Native fields, not render volumes, supply plots.
"""
import argparse,base64,html,json,math
from pathlib import Path
from coupling.review import inspect_case

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--suite',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False);summary=json.loads((a.suite/'summary.json').read_text());entries=[]
    for label in summary['cases']:
        review,rows,head=inspect_case(a.suite/label,a.output/label);entries.append((label,review,rows,head))
    from PIL import Image,ImageDraw,ImageFont
    from reportlab.graphics.shapes import Drawing,String
    from reportlab.graphics.charts.lineplots import LinePlot
    from reportlab.graphics import renderSVG
    from reportlab.lib.colors import HexColor
    font=lambda size:ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',size)
    def palette(t):
        t=max(0,min(1,t));return (int(245-220*t),int(245-125*t),int(250-40*t))
    for field,name,units in (('temperature','native-temperature-review.png','maximum over Z of native temperature rise, K'),('heat','fire-heat-review.png','Fire heat: normalized ignition proxy')):
        panels=[]
        for label,review,rows,cp in entries:
            cfg=cp['source']['checkpoint']['config'];n=cfg['width']*cfg['height']
            if field=='heat':
                import struct
                raw=base64.b64decode(cp['source']['checkpoint']['native_state_b64']);values=list(struct.unpack_from(f'<{n}f',raw,struct.calcsize('<8s4I4d16f')+3*n*4));grid=[cfg['width'],cfg['height']];extent=[(grid[0]-1)*cfg['cell_size_m'],(grid[1]-1)*cfg['cell_size_m']]
            else:
                f=cp['fluid'];g=f['configuration']['grid'];nx,ny,nz=g
                values=[max(f['fields']['temperature_k'][(k*ny+j)*nx+i]-300 for k in range(nz)) for j in range(ny) for i in range(nx)]
                grid=g[:2];extent=f['configuration']['length_m'][:2]
            panels.append((label,review,rows,values,grid,extent))
        scale=max(max(row[3]) for row in panels);img=Image.new('RGB',(1350,104+420*math.ceil(len(entries)/3)),'white');draw=ImageDraw.Draw(img)
        draw.text((24,16),units,font=font(25),fill='black');draw.text((24,52),f'Shared scale: 0 to {scale:.5g}. Raw cells/nodes; no smoothing. Different horizons are labelled.',font=font(18),fill='black')
        for index,(label,review,rows,values,grid,extent) in enumerate(panels):
            x=(index%3)*450+24;y=(index//3)*420+104;size=300
            draw.text((x,y),label,font=font(23),fill='black');draw.text((x,y+29),f"t={review['model_time_s']:g}s; return z={review['feedback']['sample_height_m']:g}m",font=font(17),fill='black')
            top=y+66
            for j in range(grid[1]):
                for i in range(grid[0]):
                    if field=='heat':
                        x0=x+max(0,i-.5)*size/(grid[0]-1);x1=x+min(grid[0]-1,i+.5)*size/(grid[0]-1)
                        y0=top+(1-min(grid[1]-1,j+.5)/(grid[1]-1))*size;y1=top+(1-max(0,j-.5)/(grid[1]-1))*size
                    else:
                        x0=x+i*size/grid[0];x1=x+(i+1)*size/grid[0];y0=top+(grid[1]-1-j)*size/grid[1];y1=top+(grid[1]-j)*size/grid[1]
                    draw.rectangle((x0,y0,x1,y1),fill=palette(values[j*grid[0]+i]/scale))
            draw.rectangle((x,top,x+size,top+size),outline='black',width=1)
            draw.text((x,top+size+5),f"X: 0 to {extent[0]:g} m; Y: 0 to {extent[1]:g} m",font=font(17),fill='black')
        img.save(a.output/name)
    colors=['#166082','#a84822','#7f498b','#377b45','#7a6200','#b31c45','#1b6674','#555555','#d66b00']
    drawing=Drawing(800,430);plot=LinePlot();plot.x=80;plot.y=65;plot.width=650;plot.height=300
    plot.data=[[(v['time_s'],v['fluid_input_energy_j']) for v in rows] for _,_,rows,_ in entries]
    plot.xValueAxis.valueMin=0;plot.xValueAxis.valueMax=2;plot.yValueAxis.valueMin=0;plot.lines.strokeWidth=1.5
    for i,color in enumerate(colors):plot.lines[i].strokeColor=HexColor(color)
    drawing.add(plot);drawing.add(String(80,397,'Integrated thermal source into Fluid (J)',fontSize=16));drawing.add(String(360,24,'Model time (s)',fontSize=12));renderSVG.drawToFile(drawing,str(a.output/'source-energy.svg'))
    table=''.join(f"<tr><td>{html.escape(label)}</td><td>{rows[-1]['time_s']:g}</td><td>{rows[-1]['fluid_input_energy_j']:.8g}</td><td>{review['feedback']['max_applied_tangent_speed_m_s']:.6g}</td><td><a href='{label}/observables.csv'>CSV</a> · <a href='{label}/authoring-review.json'>wind provenance</a></td></tr>" for label,review,rows,_ in entries)
    legend=' · '.join(f"<span style='color:{color}'>{html.escape(label)}</span>" for color,(label,_,_,_) in zip(colors,entries))
    doc=f'''<!doctype html><meta charset="utf-8"><title>Fire–Fluid qualification review</title><style>body{{font:16px system-ui;max-width:1200px;margin:30px auto;padding:0 20px;color:#18232a}}img{{max-width:100%}}td,th{{padding:8px;text-align:left;border-bottom:1px solid #ddd}}table{{border-collapse:collapse;width:100%}}</style><h1>Fire–Fluid qualification review</h1><p>{len(entries)} accepted native runs. Each passes conservation, exact replay/recovery, rollback and scene/VF3D export. Physical calibration and human visual acceptance remain unqualified.</p><table><tr><th>Case</th><th>Time, s</th><th>Thermal input, J</th><th>Maximum applied tangent speed, m/s</th><th>Evidence</th></tr>{table}</table><h2>Integrated source response</h2><p>{legend}</p><img src="source-energy.svg"><h2>Native Fluid temperature</h2><p>Top-view maximum through the vertical column, from native K fields. Shared scale includes longer runs; compare equal-duration cases when assessing sensitivity.</p><img src="native-temperature-review.png"><h2>Fire state</h2><p>Normalized heat is an ignition proxy, not temperature or enthalpy. All maps use accepted native state; render volume data is not numerical authority.</p><img src="fire-heat-review.png"><h2>Authoring findings</h2><p>Exports bind the scene and joint checkpoint. Applied wind height, hold interval and prior accepted state are now exposed in compact review sidecars. The exported Fire plane remains static, and its declared capabilities have surface-field shading disabled. No live editor controls or human visual approval are established.</p><p>Boundary profiles are explicit in each review sidecar. Open cases report advective proxy outflow; closed cases retain proxy at the patch boundary. Compare matched profiles in the accepted native maps and CSV before physical interpretation. Physical calibration needs a named fuel/source experiment, matching geometry/forcing and measured time-series with uncertainty. Current synthetic values were not fitted to a physical reference.</p>'''
    (a.output/'index.html').write_text(doc)
    (a.output/'review-index.json').write_text(json.dumps({'status':'review_artifacts_generated','cases':{label:review['digest'] for label,review,_,_ in entries},'physical_calibration_qualified':False,'human_visual_acceptance':False},indent=2)+'\n')
    print(a.output/'index.html')
if __name__=='__main__':main()
