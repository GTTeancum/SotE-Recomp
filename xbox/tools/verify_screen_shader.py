"""Check the constant ABI consumed by the Xbox renderer."""
from pathlib import Path
import re
import sys

text=Path(sys.argv[1]).read_text()
for name,index in (('position_scale',0),('texture_scale',1)):
    if not re.search(r'#var float4 '+name+r'\s+:\s+:\s+c\['+str(index)+r'\]',text):
        raise SystemExit(f'Screen shader constant layout changed: {name} must use c[{index}]')
