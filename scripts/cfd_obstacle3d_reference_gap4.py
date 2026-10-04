#!/usr/bin/env python3
"""Declared second allocation; retain all coarse-gap failures without replacement."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    for length in (8,4):
        geometry=['--length',str(length),'--center',str(length/2),'--mirror-mesh','--gaps','4']
        for count in (8,10,12,14):
            run(f'L{length}-gap4-body{count}',geometry+['--graded',str(count),'--long','4'])
        run(f'L{length}-gap4-body12-long6',geometry+['--graded','12','--long','6'])
        run(f'L{length}-gap4-empty10',geometry+['--graded','10','--long','4','--empty'])

if __name__=='__main__':main()
