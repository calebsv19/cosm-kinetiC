"""Bounded PNG presentation of coherent solver samples, using only stdlib."""
import base64
import math
import struct
import zlib


def image_content(snapshot):
    preview = snapshot.get('preview')
    if not preview or not preview.get('samples'):
        return None
    cols, rows = preview['width'], preview['height']
    grid = preview.get('grid', snapshot.get('effective_grid',[cols,rows,1]))
    eu, ev = preview.get('extent_u_m',grid[0]), preview.get('extent_v_m',grid[1])
    width = max(1,round(256*eu/max(eu,ev)))
    height = max(1,round(256*ev/max(eu,ev)))
    fields = preview.get('fields',['speed','dye','solid'])
    field = preview.get('field','speed')
    index = fields.index(field)
    stats = preview.get('statistics',{}).get(field,{'min':0,'max':preview['speed_max']})
    bounds = preview.get('color_range') or [stats['min'],stats['max']]
    low, high = bounds
    low = 0 if low is None else low
    high = low if high is None else high
    if high<=low: high=low+1e-9
    preview['display_range']=[low,high]
    preview['range_mode']='fixed' if preview.get('color_range') else 'sample_min_max'
    pixels = bytearray(width*height*3)
    def pixel(x,y,rgb):
        if 0<=x<width and 0<=y<height:pixels[(y*width+x)*3:(y*width+x)*3+3]=bytes(rgb)
    for y in range(height):
        for x in range(width):
            cell=preview['samples'][(y*rows//height)*cols+x*cols//width]
            value=cell[index]
            t=max(0,min(1,((value or 0)-low)/(high-low)))
            if value is None:rgb=(255,0,255)
            elif cell[2]:rgb=(210,218,229)
            elif low<0:
                rgb=(int(35+420*t),int(75+340*t),240) if t<.5 else (245,int(245-390*(t-.5)),int(240-430*(t-.5)))
            else:rgb=(int(30+220*t),int(45+140*t**.5),int(100+110*(1-t)))
            pixel(x,y,rgb)
    def line(x,y,ex,ey):
        steps=max(1,math.ceil(max(abs(ex-x),abs(ey-y))))
        for i in range(steps+1):pixel(round(x+(ex-x)*i/steps),round(y+(ey-y)*i/steps),(245,248,255))
    if preview.get('vectors') and len(fields)>=9:
        u,v=preview['u_axis'],preview['v_axis']
        peak=max(preview['speed_max'],1e-12)
        length=min(width/cols,height/rows)*3/peak
        preview['vector_scale']='in-plane components; maximum 3 sample cells at slice maximum speed'
        for y in range(2,rows,4):
            for x in range(2,cols,4):
                cell=preview['samples'][y*cols+x]
                if cell[2] or cell[3+u] is None or cell[3+v] is None:continue
                ax,ay=(x+.5)*width/cols,(y+.5)*height/rows
                bx,by=ax+cell[3+u]*length,ay+cell[3+v]*length
                line(ax,ay,bx,by)
                if math.hypot(bx-ax,by-ay)>2:
                    angle=math.atan2(by-ay,bx-ax)
                    for offset in (-.5,.5):line(bx,by,bx-3*math.cos(angle+offset),by-3*math.sin(angle+offset))
    raw=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    def chunk(tag,data):
        return struct.pack('!I',len(data))+tag+data+struct.pack('!I',zlib.crc32(tag+data))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',width,height,8,2,0,0,0))
    png+=chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
    return {'type':'image','mimeType':'image/png','data':base64.b64encode(png).decode()}
