"""Print original PPC instruction comments from generated guest functions."""
from pathlib import Path
import re, sys
root = Path(__file__).resolve().parent.parent
for name in sys.argv[1:]:
    needle = f'DEFINE_REX_FUNC({name})'
    for path in (root/'native/generated/default').glob('*.cpp'):
        text = path.read_text()
        start = text.find(needle)
        if start < 0: continue
        end = text.find('\nDEFINE_REX_FUNC(', start+1)
        body = text[start:end if end >= 0 else len(text)]
        print(f'\n{name} ({path.name}):')
        print('\n'.join(line.strip()[3:] for line in body.splitlines() if line.strip().startswith('// ')))
        break
