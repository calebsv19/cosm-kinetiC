"""Physical mapping of native core_sim cumulative interval fractions."""
import math
from surface_sources.receiver import mapping
from surface_sources.growth_fire_v1 import require


def allocate_window(frame, policy, window):
    weights=mapping(frame,policy)
    n=math.prod(policy['receiver']['dimensions']);steps=[]
    for part in window['steps']:
        left=part['left_n']/part['left_d'];right=part['right_n']/part['right_d']
        require(0<=left<right<=1,'core_sim overlap fraction')
        fields={k:[[] for _ in range(n)] for k in ('energy_j','smoke_kg')}
        for node,row in enumerate(weights):
            for key,source in (('energy_j','energy_transferred_j'),('smoke_kg','smoke_transferred_kg')):
                full=frame['cells'][source][node]
                # Cumulative differences telescope, including the terminal remainder.
                amount=(full if right==1 else full*right)-full*left
                require(math.isfinite(amount) and amount>=0,'source allocation precision')
                prior=[]
                for j,(cell,fraction) in enumerate(row):
                    value=amount-math.fsum(prior) if j==len(row)-1 else amount*fraction
                    require(value>=0,'spatial allocation remainder')
                    prior.append(value);fields[key][cell].append(value)
        steps.append({k:[math.fsum(values) for values in cells] for k,cells in fields.items()})
    for key,source in (('energy_j','energy_transferred_j'),('smoke_kg','smoke_transferred_kg')):
        require(math.isclose(math.fsum(math.fsum(s[key]) for s in steps),frame['totals'][source],
                            rel_tol=1e-12,abs_tol=1e-20),'whole-window transfer budget')
    return steps
