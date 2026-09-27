"""Bounded PNG presentation of a solver-owned sparse slice, using only stdlib."""
import base64
import struct
import zlib


def image_content(snapshot):
    preview=snapshot.get('preview')
    if not preview or not preview.get('samples'):
        return None
    cols,rows=preview['width'],preview['height']
    grid=snapshot['effective_grid']
    width=256
    height=max(1,min(256,round(width*grid[1]/grid[0])))
    scale=max(preview['speed_max'],1e-12)
    pixels=bytearray()
    for y in range(height):
        pixels.append(0)  # PNG row filter: none
        for x in range(width):
            speed,_,solid=preview['samples'][(y*rows//height)*cols+x*cols//width]
            t=max(0,min(1,speed/scale))
            rgb=(210,218,229) if solid else (int(30+220*t),int(45+140*t**.5),int(100+110*(1-t)))
            pixels.extend(rgb)
    def chunk(tag,data):
        return struct.pack('!I',len(data))+tag+data+struct.pack('!I',zlib.crc32(tag+data))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',width,height,8,2,0,0,0))
    png+=chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b'')
    return {'type':'image','mimeType':'image/png','data':base64.b64encode(png).decode()}
