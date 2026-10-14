"""Matched finite-volume comparisons; restrict extensive fields conservatively."""
import math
from surface_sources.growth_fire_v1 import require


def restrict(values,grid,target):
    require(len(values)==math.prod(grid),'field dimensions')
    require(all(a%b==0 for a,b in zip(grid,target)),'nested comparison grids')
    ratio=[a//b for a,b in zip(grid,target)];buckets=[[] for _ in range(math.prod(target))]
    for k in range(grid[2]):
        for j in range(grid[1]):
            for i in range(grid[0]):
                q=(k*grid[1]+j)*grid[0]+i
                t=((k//ratio[2])*target[1]+j//ratio[1])*target[0]+i//ratio[0]
                buckets[t].append(values[q])
    return [math.fsum(v) for v in buckets]


def comparison(coarse,fine,coarse_grid,fine_grid):
    rows={}
    for name in ('energy_j','smoke_kg'):
        reference=coarse[name];restricted=restrict(fine[name],fine_grid,coarse_grid)
        delta=[a-b for a,b in zip(reference,restricted)]
        scale=math.fsum(abs(v) for v in restricted)
        require(scale>0,'nonzero comparison field')
        rows[name]={'relative_l1':math.fsum(abs(v) for v in delta)/scale,
                    'relative_l2':math.sqrt(math.fsum(v*v for v in delta)/math.fsum(v*v for v in restricted)),
                    'total_difference':math.fsum(reference)-math.fsum(restricted)}
    return rows


def matched_comparisons(fields,grids):
    """Use one common observation partition for both refinement differences.

    Pair-native differences are also returned to expose unresolved fine detail.
    Ratios across different comparison partitions do not establish an order.
    """
    target=grids[0]
    common=[{key:restrict(field[key],grid,target) for key in ('energy_j','smoke_kg')}
            for field,grid in zip(fields,grids)]
    pairs=[comparison(common[i],common[i+1],target,target) for i in range(len(fields)-1)]
    native=[comparison(fields[i],fields[i+1],grids[i],grids[i+1]) for i in range(len(fields)-1)]
    return pairs,native
