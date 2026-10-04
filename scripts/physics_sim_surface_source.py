#!/usr/bin/env python3
"""Trusted-local offline general fluid/atmosphere source admission and planning."""
import argparse
import json
import sys
import sqlite3
from surface_sources.receiver import Receiver
from surface_sources.growth_fire_v1 import strict_load


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--policy',required=True);p.add_argument('--store',required=True)
    sub=p.add_subparsers(dest='operation',required=True)
    sub.add_parser('inspect')
    a=sub.add_parser('admit');a.add_argument('source')
    a=sub.add_parser('allocate');a.add_argument('--sequence',type=int,required=True)
    a.add_argument('--plan-id',required=True);a.add_argument('--boundaries',required=True,
        help='JSON file containing ordered substep boundary seconds')
    a=p.parse_args()
    try:
        receiver=Receiver(a.store,strict_load(a.policy))
        result=(receiver.admit(strict_load(a.source)) if a.operation=='admit' else
                receiver.plan(a.sequence,strict_load(a.boundaries),a.plan_id) if a.operation=='allocate'
                else receiver.inspect())
        print(json.dumps(result,sort_keys=True,allow_nan=False));return 0
    except (ValueError,OSError,sqlite3.Error) as error:
        print(json.dumps({'error':str(error)}),file=sys.stderr);return 1


if __name__=='__main__':sys.exit(main())
