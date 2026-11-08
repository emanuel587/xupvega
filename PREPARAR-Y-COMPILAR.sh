#!/usr/bin/env bash
# Preparar un VPKG PERSONAL, totalmente autónomo en Fire TV Vega OS.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
[[ -f private-config/service.json && -f private-config/device_state.json ]] || {
 echo 'ERROR: faltan service.json y device_state.json en private-config/';
 echo 'Deben ser los dos archivos de la MISMA instalación Windows que funciona.';exit 1;}
command -v python3 >/dev/null || { echo 'Instala python3';exit 1; }
command -v git >/dev/null || { echo 'Instala git: sudo apt install -y git';exit 1; }
command -v cmake >/dev/null || { echo 'Instala cmake: sudo apt install -y cmake';exit 1; }
command -v vega >/dev/null || { [[ -f "$HOME/vega/env" ]] && source "$HOME/vega/env"; }
command -v vega >/dev/null || { echo 'Vega SDK no está en PATH';exit 1; }
TARGET="$ROOT/XuperTVNF-vega-project"
if [[ ! -f "$TARGET/manifest.toml" ]]; then
 echo '[1/7] Generando el proyecto Vega...'
 vega project generate --template vegaWebview --name XuperTVNF \
  --packageId com.synovasystems.xupertvnf --outputDir "$TARGET"
fi
mkdir -p "$TARGET/src/turbo-modules" "$TARGET/assets/image" "$TARGET/assets/private" "$TARGET/native-service" "$TARGET/kepler/turbo-modules"
# Key for local HTTP requests; generated on user's PC, NOT embedded in this ZIP.
if [[ ! -f "$ROOT/native-service/local_key.hpp" ]]; then
 python3 - <<'PYKEY'
import secrets,json
from pathlib import Path
key=secrets.token_hex(24)
Path('native-service/local_key.hpp').write_text('#pragma once\n#define XUPER_LOCAL_KEY "'+key+'"\n#define XUPER_LOCAL_PORT 8139\n')
Path('assets/bridge.json').write_text(json.dumps({'host':'http://127.0.0.1:8139','key':key})+'\n')
PYKEY
fi
# Ensure host/key stay matched and look sane even after partial earlier builds.
python3 - <<'PYVALIDATE'
import json,re
from pathlib import Path
bridge=json.loads(Path('assets/bridge.json').read_text())
key=Path('native-service/local_key.hpp').read_text()
assert bridge['host']=='http://127.0.0.1:8139'
assert re.fullmatch('[a-f0-9]{48}',bridge['key'])
assert bridge['key'] in key,'Error: clave nativa y JS no coinciden'
for name in ('service.json','device_state.json'):
    src=Path('private-config')/name
    value=json.loads(src.read_text())
    assert isinstance(value,dict),f'JSON inválido: {name}'
assert 'portal_hosts' in json.loads(Path('private-config/service.json').read_text()),'service.json no tiene portal_hosts'
PYVALIDATE

echo '[2/7] Copiando archivos privados a una compilación PERSONAL...'
for name in service.json device_state.json;do
 cp "private-config/$name" "$TARGET/assets/private/$name"
 chmod 600 "$TARGET/assets/private/$name"
done
cp "$ROOT/assets/index.html" "$TARGET/assets/index.html"
cp "$ROOT/assets/bridge.json" "$TARGET/assets/bridge.json"
cp "$ROOT/assets/cacert.pem" "$TARGET/assets/cacert.pem"
cp "$ROOT/assets/xuper-icon.png" "$TARGET/assets/xuper-icon.png"
cp "$ROOT/assets/image/xuper-icon.png" "$TARGET/assets/image/xuper-icon.png"
cp "$ROOT/App.tsx" "$TARGET/App.tsx"
cp "$ROOT/src/"*.tsx "$TARGET/src/"
cp "$ROOT/src/"*.ts "$TARGET/src/"
cp "$ROOT/src/turbo-modules/NativeXuperDevice.ts" "$TARGET/src/turbo-modules/"
cp "$ROOT/native-service/"*.cpp "$ROOT/native-service/"*.hpp "$TARGET/native-service/"
cp "$ROOT/native-service/local_key.hpp" "$TARGET/native-service/"
cp "$ROOT/kepler/AutoLinkInit.cpp" "$TARGET/kepler/"
cp "$ROOT/CMakeLists.txt" "$TARGET/"
python3 - "$TARGET/manifest.toml" <<'PYMANIFEST'
from pathlib import Path
import sys,re
p=Path(sys.argv[1]);s=p.read_text()
block=re.search(r'(?ms)^\[package\].*?(?=^\[|\Z)',s)
if not block:raise SystemExit('Falta [package] en manifest.toml')
old=block.group(0)
for k,v in {'title':'Xuper TV NF','icon':'@image/xuper-icon.png'}.items():
    if re.search(r'^'+k+r'\s*=.*$',old,re.M):
        old=re.sub(r'^'+k+r'\s*=.*$',f'{k} = "{v}"',old,flags=re.M)
    else:old+=f'{k} = "{v}"\n'
s=s[:block.start()]+old+s[block.end():]
# loopback URL is not exposed to the LAN; make HTTP explicitly local only.
s=re.sub(r'(?ms)^\[network-traffic-policy\.cleartext\]\n.*?(?=^\[|\Z)','',s)
s+='\n[network-traffic-policy.cleartext]\nallowed-domains = ["127.0.0.1"]\n'
# Native player services required by Vega.
for service in ['com.amazon.webview.renderer_service','com.amazon.mediametrics.service',
 'com.amazon.mediabuffer.service','com.amazon.mediatransform.service','com.amazon.audio.stream',
 'com.amazon.audio.control','com.amazon.audio.system','com.amazon.media.server',
 'com.amazon.media.playersession.service']:
    if not re.search(r'^id\s*=\s*["\']'+re.escape(service)+r'["\']',s,re.M):
        s+='\n[[wants.service]]\nid = "'+service+'"\n'
if 'com.amazon.devconf.privilege.accessibility' not in s:
    s+='\n[[wants.privilege]]\nid = "com.amazon.devconf.privilege.accessibility"\n'
p.write_text(s)
PYMANIFEST

echo '[3/7] Dependencias compatibles con Vega SDK 0.24...'
node - "$TARGET/package.json" <<'JS'
const fs=require('fs'),p=process.argv[2],a=JSON.parse(fs.readFileSync(p,'utf8'));
a.dependencies={...a.dependencies,
 '@amazon-devices/react-native-w3cmedia':'^2.3.5',
 '@amazon-devices/keplerscript-turbomodule-api':'*'};
a.devDependencies={...a.devDependencies, '@babel/plugin-transform-react-jsx':'^7.25.0'};
a.kepler=a.kepler||{};
a.kepler.keplerInitializationSharedLibrary='com.synovasystems.xupernative';
a.kepler.autolink=a.kepler.autolink||{};
a.kepler.autolink['com.synovasystems.xupernative']={libraryName:'libxupernative.so',provider:'application',linkDynamic:true,turbomodules:['XuperDevice']};
fs.writeFileSync(p,JSON.stringify(a,null,2)+'\n');
JS
cat > "$TARGET/babel.config.js" <<'JS'
module.exports={presets:[['module:metro-react-native-babel-preset',{useTransformReactJSXExperimental:true}]],plugins:[['@babel/plugin-transform-react-jsx',{runtime:'automatic'}]]};
JS
(cd "$TARGET" && vega project update-manifest --os-min 1.2 --os-version 1.2)
echo '[4/7] Instalando paquetes de Node...'
(cd "$TARGET" && npm install)
echo '[5/7] Generando Turbo Module nativo...'
rm -rf "$TARGET/kepler/turbo-modules"
(cd "$TARGET" && npx keplerscript-turbomodule-api codegen src/turbo-modules/NativeXuperDevice.ts -o kepler/turbo-modules/ --new --namespace XuperDeviceNative)
(cd "$TARGET" && python3 "$ROOT/tools/completar_turbomodulo.py")
echo '[6/7] Compilando C++ para Vega + dependencias criptográficas...'
# FetchContent fetches known versions of MbedTLS + curl and cross-compiles them
# with Vega's own toolchain. Ubuntu's libcurl/mbedtls .so are never copied.
(cd "$TARGET" && npm run build:app)
echo '[7/7] Compilación terminada. Paquetes disponibles:'
if [[ -d "$TARGET/build" ]]; then
  find "$TARGET/build" -type f -name '*.vpkg' -print
  if ! find "$TARGET/build" -type f -name '*.vpkg' | grep -q .; then
    echo 'ERROR: finalizó la construcción pero no se encontró ningún .vpkg.'
    exit 1
  fi
else
  echo 'ERROR: no se generó la carpeta de compilación.'
  exit 1
fi
echo 'AVISO: este VPKG incluye credenciales PRIVADAS. Solo para tu Fire TV. NO lo compartas.'
