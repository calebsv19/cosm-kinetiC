#!/usr/bin/env python3
"""Same physical near-body X nodes in short and long reference domains."""
from cfd_obstacle3d_reference_refinement import run,DATA

def main():
    DATA.mkdir(parents=True,exist_ok=True)
    geometry=['--length','4','--center','2','--mirror-mesh','--gaps','4','--long','6','--physical-long-grid']
    for count in (10,12,14):
        run(f'L4-matched{count}',geometry+['--graded','12','--body-counts',f'12,{count},{count}'])
    run('L4-matched-empty10',geometry+['--graded','10','--empty'])

if __name__=='__main__':main()
