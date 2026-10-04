#!/usr/bin/env python3
"""Preserve fine X-normal intervals and refine both transverse body directions."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    for length in (8,4):
        geometry=['--length',str(length),'--center',str(length/2),'--mirror-mesh','--gaps','4','--long','6']
        for count in (8,10,12,14):
            run(f'L{length}-directional{count}',geometry+['--graded','8','--body-counts',f'8,{count},{count}'])
        run(f'L{length}-directional-empty10',geometry+['--graded','10','--empty'])

if __name__=='__main__':main()
