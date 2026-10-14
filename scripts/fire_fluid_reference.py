#!/usr/bin/env python3
"""Inspect downloaded, hash-pinned methanol reference without fitting the model."""
import argparse,json
from pathlib import Path
from coupling.methanol_reference import inspect

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('reference','config','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();value=inspect(a.reference,json.loads(a.config.read_text()))
    with a.output.open('x') as f:f.write(json.dumps(value,indent=2)+'\n')
    print(json.dumps({key:value[key] for key in ('material','steady_hrr_w','steady_mass_loss_kg_s','peak_reference_gas_temperature_k','current_model_compatible','physical_calibration_qualified')},indent=2))
