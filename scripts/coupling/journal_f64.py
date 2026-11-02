"""Lossless journal-only f64 packing. IEEE754 bits and JSON types are preserved."""
import array
import base64
import sys
from surface_sources.growth_fire_v1 import require
TAG='$journal_f64_le_shuffle_v1'
REF='$journal_state_field_v1'
ARRAY_FIELDS=('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','boundary_fluxes')
def pack(value):
    if type(value) is dict:
        require(TAG not in value and REF not in value,'reserved journal tag collision')
        # A result contains identical field/state arrays. Replace just verified
        # duplicates; restore them before any digest or physical validation.
        if 'fluid' in value and type(value['fluid']) is dict:
            fluid=value['fluid'];fields=fluid.get('fields',{});data=fluid.get('state',{}).get('data',{})
            require(all(fields.get(k)==data.get(k) for k in ARRAY_FIELDS),'journal field/state identity')
            reduced={**value,'fluid':{**fluid,'fields':{k:({REF:k} if k in ARRAY_FIELDS else v) for k,v in fields.items()}}}
            return {k:pack(v) if k!='fluid' else _fluid_pack(v) for k,v in reduced.items()}
        return {k:pack(v) for k,v in value.items()}
    if type(value) is list:
        if len(value)>=256 and all(type(v) is float for v in value):
            binary=array.array('d',value)
            if sys.byteorder!='little':binary.byteswap()
            raw=binary.tobytes();shuffled=b''.join(raw[a::8] for a in range(8))
            return {TAG:base64.b64encode(shuffled).decode('ascii'),'count':len(value)}
        return [pack(v) for v in value]
    return value

def _fluid_pack(fluid):
    # Generated references bypass the user-value reserved-tag refusal.
    result={k:pack(v) for k,v in fluid.items() if k!='fields'}
    result['fields']={k:v if type(v) is dict and REF in v else pack(v) for k,v in fluid['fields'].items()}
    return result

def unpack(value):
    if type(value) is dict:
        if TAG in value:
            require(set(value)=={TAG,'count'} and type(value['count']) is int and 256<=value['count']<=4000000,'journal f64 shape')
            data=base64.b64decode(value[TAG],validate=True)
            require(len(data)==8*value['count'],'journal f64 byte count')
            raw=bytearray(len(data));count=value['count']
            for a in range(8):raw[a::8]=data[a*count:(a+1)*count]
            binary=array.array('d');binary.frombytes(raw)
            if sys.byteorder!='little':binary.byteswap()
            return binary.tolist()
        result={k:unpack(v) for k,v in value.items()}
        if 'fluid' in result and type(result['fluid']) is dict:
            fields=result['fluid'].get('fields',{});data=result['fluid'].get('state',{}).get('data',{})
            for k in ARRAY_FIELDS:
                require(fields.get(k)=={REF:k},'journal field reference')
                fields[k]=list(data[k])
        return result
    if type(value) is list:return [unpack(v) for v in value]
    return value
