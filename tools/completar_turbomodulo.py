#!/usr/bin/env python3
"""Fills the generated, SDK-version-dependent Turbo Module launch stub.
We never patch generated *spec* code, only the class implementation stub.
"""
import re
from pathlib import Path
candidates=list(Path('kepler/turbo-modules').glob('*.cpp'))
matches=[p for p in candidates if 'XuperDevice::launch' in p.read_text()]
if len(matches)!=1:
    raise SystemExit('ERROR: codegen no generó una implementación única XuperDevice::launch().')
p=matches[0]
s=p.read_text()
# Look for the C++ implementation signature, ignoring the return type chosen by codegen.
m=re.search(r'\bXuperDevice::launch\s*\([^)]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{',s)
if not m:
    raise SystemExit('ERROR: codegen cambió la firma de launch(); no se puede parchear de forma segura.')
start=m.end()-1
# Include braces in comments and literals are rare in a single generated stub;
# use lexically conservative brace matching for its body.
level=0;end=None
for i in range(start,len(s)):
    if s[i]=='{':level+=1
    elif s[i]=='}':
        level-=1
        if level==0:end=i+1;break
if end is None:raise SystemExit('ERROR: cuerpo de launch() incompleto.')
body="{\n  return xuper_launch_on_device(XUPER_LOCAL_KEY, XUPER_LOCAL_PORT);\n}"
s=s[:start]+body+s[end:]
s='#include "on_device.hpp"\n#include "local_key.hpp"\n'+s
p.write_text(s)
print('OK: launch() de Turbo Module conectado con servicio interno C++')
