"""Bounded read-only ZIP verification against an exact ordinary app inventory.

AppleDouble structure is checked; full ACL/xattr/authentication identity is not.
No extraction or archive-member execution occurs.
"""
import hashlib
import os
from pathlib import Path
import stat
import struct
import time
import zipfile
import zlib

from desktop_replace import inventory

LIMITS = {'max_entries':100000, 'max_central_bytes':67108864,
          'max_bytes':34359738368, 'max_file_bytes':8589934592,
          'max_metadata_bytes':16777216, 'wall_cap':120}


def central_bounds(stream, size, limits, check):
    stream.seek(max(0,size-65557));tail=stream.read(65557)
    index=tail.rfind(b'PK\x05\x06')
    if index<0 or len(tail)-index<22:raise ValueError('Missing ZIP end record')
    signature,disk,start,count_disk,count,central_size,offset,comment=struct.unpack('<4s4H2LH',tail[index:index+22])
    end=size-len(tail)+index
    if index+22+comment!=len(tail):raise ValueError('ZIP end record/comment mismatch')
    if disk or start or count_disk!=count:raise ValueError('Multi-disk ZIP is unsupported')
    if count==65535 or central_size==4294967295 or offset==4294967295:
        if end<20:raise ValueError('Missing ZIP64 locator')
        stream.seek(end-20);locator=stream.read(20)
        magic,number,position,disks=struct.unpack('<4sLQL',locator)
        if magic!=b'PK\x06\x07' or number or disks!=1:raise ValueError('Invalid ZIP64 locator')
        if position<0 or position+56>end-20:raise ValueError('Invalid ZIP64 end offset')
        stream.seek(position);header=stream.read(56)
        magic,length,made,needed,disk,start,count_disk,count,central_size,offset=struct.unpack('<4sQ2H2L4Q',header)
        if magic!=b'PK\x06\x06' or not 44<=length<=4096 or position+12+length>end-20:
            raise ValueError('Invalid ZIP64 end record')
        if disk or start or count_disk!=count:raise ValueError('Multi-disk ZIP64 is unsupported')
        end=position
    if count>limits['max_entries'] or central_size>limits['max_central_bytes']:
        raise ValueError('ZIP central-directory bound reached')
    if offset+central_size>end:raise ValueError('ZIP central directory escapes end record')
    stream.seek(offset);finish=offset+central_size;observed=0
    while stream.tell()<finish:
        check()
        if observed>=limits['max_entries']:raise ValueError('ZIP central entry bound reached')
        header=stream.read(46)
        if len(header)!=46:raise ValueError('Truncated ZIP central member')
        fields=struct.unpack('<4s6H3L5H2L',header)
        if fields[0]!=b'PK\x01\x02' or fields[13]!=0:raise ValueError('Invalid ZIP central member')
        if fields[10]>4096:raise ValueError('ZIP central name bound reached')
        length=sum(fields[10:13])
        if stream.tell()+length>finish:raise ValueError('ZIP central member escapes directory')
        stream.seek(length,1);observed+=1
    if observed!=count:raise ValueError('ZIP central member count mismatch')
    return count


def apple_double(payload):
    if len(payload)<26:raise ValueError('Truncated AppleDouble metadata')
    magic,version,filler,count=struct.unpack('>LL16sH',payload[:26])
    if magic!=0x00051607 or version!=0x00020000 or count>4096 or 26+count*12>len(payload):
        raise ValueError('Invalid AppleDouble header')
    entries=[];identifiers=set();minimum=26+count*12
    for index in range(count):
        identity,offset,size=struct.unpack('>LLL',payload[26+12*index:38+12*index])
        if identity in identifiers or offset<minimum or offset+size>len(payload):
            raise ValueError('Invalid AppleDouble entry')
        identifiers.add(identity)
        if size:entries.append((offset,offset+size))
    entries.sort()
    if any(left[1]>right[0] for left,right in zip(entries,entries[1:])):
        raise ValueError('Overlapping AppleDouble entries')


def deflate_end(stream, info, check, remaining_budget):
    """Check a complete raw Deflate stream; ZipExtFile can ignore missing EOF."""
    stream.seek(info.header_offset);header=stream.read(30)
    if len(header)!=30:raise ValueError('Truncated ZIP local header')
    fields=struct.unpack('<4s5H3L2H',header)
    if fields[0]!=b'PK\x03\x04' or fields[2]!=info.flag_bits or fields[3]!=zipfile.ZIP_DEFLATED:
        raise ValueError('ZIP local/central compression mismatch')
    stream.seek(fields[9]+fields[10],1)
    decoder=zlib.decompressobj(-15);compressed=info.compress_size;expanded=0
    while compressed:
        check();block=stream.read(min(1048576,compressed))
        if not block:raise ValueError('Truncated ZIP compressed stream')
        compressed-=len(block)
        while block:
            check();out=decoder.decompress(block,1048576);expanded+=len(out)
            if expanded>info.file_size or expanded>remaining_budget:
                raise ValueError('ZIP decompression work bound reached')
            if decoder.unused_data:raise ValueError('ZIP Deflate stream has trailing data')
            block=decoder.unconsumed_tail
    if not decoder.eof or expanded!=info.file_size:
        raise ValueError('ZIP Deflate stream lacks a valid end marker')
    return expanded


def verify(archive, app, *, limits=None):
    limits=dict(LIMITS if limits is None else limits)
    if set(limits)!=set(LIMITS):raise ValueError('Invalid ZIP bound selection')
    for name,value in limits.items():
        if type(value) is not int or not 1<=value<=LIMITS[name]:raise ValueError('Invalid ZIP '+name+' bound')
    started=time.monotonic()
    def check():
        if time.monotonic()-started>=limits['wall_cap']:raise ValueError('ZIP wall bound reached')
    app=Path(app);archive=Path(archive)
    expected=inventory(app);before=inventory(archive)['.']
    if before['kind']!='file' or before['bytes']>limits['max_file_bytes']:
        raise ValueError('ZIP archive byte bound reached')
    descriptor=os.open(archive,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    total=0;logical_total=0;observed=set();names=set();metadata_count=0
    def identity(info):return (info.st_dev,info.st_ino,info.st_mode,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
    try:
        initial=os.fstat(descriptor)
        if not stat.S_ISREG(initial.st_mode) or initial.st_size!=before['bytes']:
            raise ValueError('ZIP must be regular with stable size')
        with os.fdopen(descriptor,'rb',closefd=False) as stream:
            count=central_bounds(stream,initial.st_size,limits,check);check()
            with zipfile.ZipFile(stream) as payload:
                infos=payload.infolist()
                if len(infos)!=count or len(infos)>limits['max_entries']:raise ValueError('ZIP member count mismatch')
                for info in infos:
                    check();name=info.filename
                    if info.orig_filename!=name or len(name.encode())>4096 or '\\' in name:
                        raise ValueError('Unsafe ZIP member name')
                    selected=name[:-1] if name.endswith('/') else name
                    parts=selected.split('/')
                    if not selected or any(part in ('','.','..') for part in parts):raise ValueError('Unsafe ZIP member path')
                    if selected in names:raise ValueError('Duplicate ZIP member')
                    names.add(selected)
                    if info.flag_bits&65 or info.compress_type not in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED):
                        raise ValueError('Encrypted or unsupported ZIP compression')
                    mode=info.external_attr>>16;kind=stat.S_IFMT(mode)
                    metadata=parts[0]=='__MACOSX'
                    if metadata:
                        if info.is_dir():
                            if kind!=stat.S_IFDIR:raise ValueError('ZIP metadata directory type mismatch')
                            if parts!=['__MACOSX']:
                                relative='/'.join(parts[2:]) or '.'
                                if len(parts)<2 or parts[1]!=app.name or expected.get(relative,{}).get('kind')!='directory':
                                    raise ValueError('Foreign ZIP metadata directory')
                        else:
                            if len(parts)<2 or not parts[-1].startswith('._') or not parts[-1][2:]:
                                raise ValueError('Foreign ZIP metadata payload')
                            original=parts[1:-1]+[parts[-1][2:]]
                            if not original or original[0]!=app.name:
                                raise ValueError('Foreign AppleDouble root')
                            relative='/'.join(original[1:]) or '.'
                            if relative not in expected:raise ValueError('AppleDouble target absent from app')
                            if kind!=stat.S_IFREG or info.file_size>limits['max_metadata_bytes']:
                                raise ValueError('ZIP metadata byte/type bound reached')
                        row=None
                    else:
                        if parts[0]!=app.name:raise ValueError('ZIP contains foreign app payload')
                        relative='/'.join(parts[1:]) or '.';row=expected.get(relative)
                        if row is None:raise ValueError('Unexpected ZIP app member')
                        required={'file':stat.S_IFREG,'directory':stat.S_IFDIR,'symlink':stat.S_IFLNK}[row['kind']]
                        if kind!=required or stat.S_IMODE(mode)!=row['mode'] or info.is_dir()!=(row['kind']=='directory'):
                            raise ValueError('ZIP app member type/mode mismatch')
                        observed.add(relative)
                        if row['kind']=='file' and info.file_size!=row['bytes']:
                            raise ValueError('ZIP app byte count differs from signed input')
                        if row['kind']=='symlink' and info.file_size!=len(os.fsencode(row['target'])):
                            raise ValueError('ZIP link byte count differs from signed input')
                    if info.file_size>limits['max_file_bytes'] or total+info.file_size>limits['max_bytes']:
                        raise ValueError('ZIP expanded byte bound reached')
                    if info.is_dir() and info.file_size:raise ValueError('ZIP directory contains payload')
                    result=hashlib.sha256();chunks=[];size=0
                    with payload.open(info) as member:
                        while True:
                            check();block=member.read(1048576)
                            if not block:break
                            size+=len(block);total+=len(block)
                            if size>limits['max_file_bytes'] or total>limits['max_bytes']:raise ValueError('ZIP expanded byte bound reached')
                            result.update(block)
                            if metadata and not info.is_dir() or row and row['kind']=='symlink':chunks.append(block)
                    if size!=info.file_size:raise ValueError('ZIP expanded size mismatch')
                    logical_total+=size
                    if info.compress_type==zipfile.ZIP_DEFLATED:
                        total+=deflate_end(stream,info,check,limits['max_bytes']-total)
                    if metadata and not info.is_dir():apple_double(b''.join(chunks));metadata_count+=1
                    elif row and row['kind']=='file':
                        if size!=row['bytes'] or result.hexdigest()!=row['sha256']:raise ValueError('ZIP app bytes differ from signed input')
                    elif row and row['kind']=='symlink':
                        if b''.join(chunks)!=os.fsencode(row['target']):raise ValueError('ZIP link differs from signed input')
        if identity(os.fstat(descriptor))!=identity(initial) or identity(archive.lstat())!=identity(initial):
            raise ValueError('ZIP changed during readback')
    except (zipfile.BadZipFile,NotImplementedError,struct.error,zlib.error) as error:
        raise ValueError('Invalid ZIP structure: '+str(error)) from error
    finally:os.close(descriptor)
    if observed!=set(expected):raise ValueError('ZIP is missing signed app members')
    check()
    if inventory(app)!=expected or inventory(archive)['.']!=before:raise ValueError('ZIP or app identity changed during verification')
    return {'status':'verified_ordinary_app_payload','archive_sha256':before['sha256'],
            'app_members':len(observed),'appledouble_members':metadata_count,'expanded_bytes':logical_total,'decompressed_work_bytes':total,
            'complete_metadata_identity_verified':False}
