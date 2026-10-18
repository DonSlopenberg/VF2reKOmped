"""Catch visible SEGA branding in early GPU frames, not just the skip flag."""
from pathlib import Path
import sys, numpy as np
folder=Path(sys.argv[1]);log=(folder/'runtime.log').read_text(errors='replace')
frames=folder/'captures'
counts=[]
for i in range(1,7):
    image=np.frombuffer((frames/f'{i}.rgba').read_bytes(),np.uint8).reshape(720,1280,4)
    # Exact dominant ink of the original, unfiltered startup logo.
    counts.append(int(np.all(image[:,:,:3]==[0,35,113],axis=2).sum()))
assert max(counts)<1000, f'Visible startup logo pixels: {counts}'
assert 'suppressed registered branding animation' in log
assert 'opening native Main Menu' in log
print(f'PASS: logo pixel counts {counts}; native Main Menu reached')
