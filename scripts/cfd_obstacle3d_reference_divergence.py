#!/usr/bin/env python3
"""Consistent divergence robustness and pressure Schur scaling controls."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    # Unchanged gamma0 equations; exact same existing directional geometry.
    run('L8-directional14-true-stop',['--length','8','--center','4','--mirror-mesh','--graded','8',
         '--body-counts','8,14,14','--gaps','4','--long','6'])
    for length in (4,8):
        geometry=['--length',str(length),'--center',str(length/2),'--mirror-mesh','--gaps','4','--long','6',
                  '--graded',str(12 if length==4 else 8),'--body-counts',f'{12 if length==4 else 8},14,14']
        if length==4:geometry+=['--physical-long-grid']
        for gamma,tag in (('.1','gd01'),('1','gd1')):
            run(f'L{length}-divergence-{tag}',geometry+['--grad-div',gamma])

if __name__=='__main__':main()
