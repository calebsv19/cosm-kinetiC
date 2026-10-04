#!/usr/bin/env python3
"""True first-interval refinement; every physical normal node is retained."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    for length,count in ((4,12),(8,8)):
        run(f'L{length}-normal-split',['--length',str(length),'--center',str(length/2),'--mirror-mesh',
            '--graded',str(count),'--body-counts',f'{count},12,12','--gaps','4','--long','6',
            '--physical-long-grid','--split-first-normal'])
    # Finish the admitted empty calibration omitted by the earlier process
    # which had already loaded its predecessor command sequence.
    for length in (4,8):
        run(f'L{length}-empty14',['--length',str(length),'--center',str(length/2),'--mirror-mesh',
            '--graded','14','--gaps','3','--long','3','--empty'])

if __name__=='__main__':main()
