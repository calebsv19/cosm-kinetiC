#!/usr/bin/env python3
"""Final bounded gamma=mu normal-refinement test; no further allocation sweep."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    for length,count in ((4,12),(8,8)):
        run(f'L{length}-normal-split-gd01',['--length',str(length),'--center',str(length/2),'--mirror-mesh',
            '--graded',str(count),'--body-counts',f'{count},12,12','--gaps','4','--long','6',
            '--physical-long-grid','--split-first-normal','--grad-div','.1'])
        geometry=['--length',str(length),'--center',str(length/2),'--mirror-mesh',
                  '--graded',str(count),'--body-counts',f'{count},12,12','--gaps','4','--long','6','--grad-div','.1']
        if length==4:geometry+=['--physical-long-grid']
        run(f'L{length}-normal-unsplit-gd01',geometry)

if __name__=='__main__':main()
