"""Bounded local sparse-forcing qualification of the existing native atmosphere.

PSQUAL01/PSCPQ001 are app-private little-endian binary64 experiment packets.
Explicit movie mode selects 32 seconds/6400 steps/160 samples; the original
qualification mode keeps 8 seconds/3200 steps/3 samples. Every decoded sample
passes the ordinary independent physical acceptance gates.
Separate domain qualification retains the eight-second envelope while admitting
up to 128 vertical cells and 524288 total cells.
Explicit domain movie mode admits 40 seconds/8000 steps/200 samples on that
tall grid, with a separately bounded forcing file and scalar-work budget.
"""
import array
import hashlib
import json
import math
import struct
import subprocess
import sys
import time
from pathlib import Path
from open_atmosphere import accept_fields, validate, native_strict_load
from surface_sources.growth_fire_v1 import integer, number, require

FORCING = struct.Struct('<8s5Id')
CELL = struct.Struct('<Idd')
SAMPLE = struct.Struct('<8s4I8dQ')
LIMIT = 64*1024*1024

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def write_schedule(path, request, steps, sample_steps, *, movie=False,domain_qualification=False,domain_movie=False):
    """Encode verified sparse allocations; preserve each binary64 amount exactly."""
    validate(dict(request, state=None, steps=[]),movie=movie,domain_qualification=domain_qualification,domain_movie=domain_movie)
    integer(len(steps), 1, 8000 if domain_movie else 6400 if movie else 3200)
    require(1 <= len(sample_steps) <= (200 if domain_movie else 160 if movie else 3) and sample_steps[-1] == len(steps), 'samples')
    require(sample_steps == sorted(set(sample_steps)), 'ordered unique samples')
    for value in sample_steps: integer(value, 1, len(steps))
    require(len(steps)*request['momentum_dt_s'] <= (40 if domain_movie else 32 if movie else 8), 'physical duration bound')
    n = math.prod(request['grid']); totals = []; accumulated = [[], []]
    with Path(path).open('xb') as file:
        file.write(FORCING.pack(b'PSQUAL01', *request['grid'], len(steps),
                                len(sample_steps), request['momentum_dt_s']))
        file.write(struct.pack('<'+'I'*len(sample_steps), *sample_steps))
        for step in steps:
            cells = sorted(step, key=lambda cell: cell['cell_index'])
            require(len(cells) <= n, 'sparse count')
            file.write(struct.pack('<I', len(cells))); previous = -1
            amounts = [[], []]
            for cell in cells:
                index = cell['cell_index']; integer(index, 0, n-1)
                require(index > previous, 'duplicate source cell'); previous = index
                energy = cell['energy_transferred_j']; smoke = cell['smoke_transferred_kg']
                number(energy, 0, 1e12); number(smoke, 0, 1e12)
                file.write(CELL.pack(index, energy, smoke))
                amounts[0].append(energy); amounts[1].append(smoke)
            for q in range(2): accumulated[q].append(math.fsum(amounts[q]))
            totals.append({'energy_j': math.fsum(accumulated[0]),
                           'smoke_kg': math.fsum(accumulated[1])})
            require(file.tell() <= (4*LIMIT if domain_movie else LIMIT), 'sparse forcing byte bound')
    return totals

def run_sparse(request, worker, schedule, output, *, timeout_s=3600,movie=False,domain_qualification=False,domain_movie=False):
    """Closed qualification/movie selection; ordinary run keeps its 120s cap."""
    validate(dict(request, state=None, steps=[]),movie=movie,domain_qualification=domain_qualification,domain_movie=domain_movie)
    require(type(timeout_s) is int and 1 <= timeout_s <= (21600 if domain_movie else 14400 if movie else 7200), 'qualification/movie timeout')
    output = Path(output).resolve(); output.mkdir()
    worker = Path(worker).resolve(); worker_sha = sha(worker); forcing_sha = sha(schedule)
    configuration = output/'request.json'
    configuration.write_text(json.dumps(dict(request, state=None, steps=[]),
                                        allow_nan=False, separators=(',', ':')))
    require(configuration.stat().st_size <= LIMIT, 'qualification config byte bound')
    started = time.monotonic()
    with (output/'stdout.json').open('wb') as stdout, (output/'stderr.log').open('wb') as stderr:
        completed = subprocess.run([str(worker), str(configuration), '--sparse-domain-movie' if domain_movie else '--sparse-domain-qualification' if domain_qualification else '--sparse-movie' if movie else '--sparse-qualification',
            str(Path(schedule).resolve()), '--samples-root', str(output)],
            stdout=stdout, stderr=stderr, timeout=timeout_s)
    require(sha(worker) == worker_sha and sha(schedule) == forcing_sha, 'pinned inputs changed')
    require(completed.returncode == 0, 'native exit '+str(completed.returncode)+': '+
            (output/'stderr.log').read_text()[-3000:])
    receipt = native_strict_load(output/'stdout.json', 64*1024)
    require(receipt['schema'] == ('physics_sim_sparse_domain_movie_receipt/v1' if domain_movie else 'physics_sim_sparse_domain_receipt/v1' if domain_qualification else 'physics_sim_sparse_movie_receipt/v1' if movie else 'physics_sim_sparse_qualification_receipt/v1'), 'native receipt')
    with Path(schedule).open('rb') as file:
        header = FORCING.unpack(file.read(FORCING.size))
    require(receipt['steps'] == header[4] and receipt['time_s'] == header[4]*header[6],
            'completed qualification physical clock')
    for name in ('numerical_peak_bytes','peak_divergence_s_inv',
                 'peak_projection_relative_residual','peak_temperature_k',
                 'peak_velocity_component_m_s'):
        number(receipt[name], 0, 1e12)
    receipt.update(worker_sha256=worker_sha, forcing_sha256=forcing_sha,
                   native_elapsed_s=time.monotonic()-started)
    return receipt

def read_sample(path, request, worker_sha, source_totals, *, numerical_peak_bytes=0,movie=False,compact=False,domain_qualification=False,domain_movie=False):
    """Convert a lossless native packet into the existing accepted result schema."""
    path = Path(path); require(path.stat().st_size <= LIMIT, 'sample byte bound')
    raw = path.read_bytes(); require(len(raw) >= SAMPLE.size, 'sample header')
    h = SAMPLE.unpack_from(raw)
    grid = tuple(request['grid']); n = math.prod(grid); plane = grid[0]*grid[1]
    require(h[0] == b'PSCPQ001' and h[1:4] == grid and h[6] == request['momentum_dt_s'],
            'sample grid/dt contract')
    count = h[4]; require(h[5] == count*h[6], 'sample physical clock')
    sizes = [3*n+plane, n, n, n, 8*plane]
    require(len(raw) == SAMPLE.size+8*sum(sizes), 'sample payload length')
    values = array.array('d'); values.frombytes(raw[SAMPLE.size:])
    if sys.byteorder != 'little': values.byteswap()
    data = {}; offset = 0
    for key,size in zip(('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','boundary_fluxes'), sizes):
        data[key] = values[offset:offset+size].tolist(); offset += size
    data.update(steps=count, time_s=h[5], input_energy_j=h[7], input_smoke_kg=h[8],
        initial_energy_j=h[9], initial_smoke_kg=h[10], scalar_work_cells=h[13])
    volume = math.prod(length/count for length,count in zip(request['length_m'],grid))
    props = request['properties']; capacity = props['density_kg_m3']*props['heat_capacity_j_kg_k']*volume
    fields = {**{key:data[key] for key in ('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','boundary_fluxes')},
        'schema':'physics_sim_open_atmosphere_fields/v1', 'state':data,
        'start_time_s':0, 'time_s':h[5], 'max_divergence_s_inv':h[11],
        'projection_relative_residual':h[12], 'numerical_peak_bytes':numerical_peak_bytes,
        'temperature_k':[props['reference_temperature_k']+v/capacity for v in data['energy_j']],
        'smoke_concentration_kg_m3':[v/volume for v in data['smoke_kg']]}
    return accept_fields(dict(request,state=None,steps=[]), fields, worker_sha,
                         new_step_count=count, source_totals=source_totals,movie=movie,compact=compact,domain_qualification=domain_qualification,domain_movie=domain_movie)
