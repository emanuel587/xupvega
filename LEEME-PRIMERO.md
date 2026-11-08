# Xuper TV NF — Port autónomo para Fire TV Vega OS

**Estado: código experimental de migración nativa; todavía NO está compilado ni validado en tu Fire TV.**

## Qué cambia

- Conserva el HTML/CSS/JS original de Xuper TV NF.
- El backend C++ del catálogo se empaqueta en un Turbo Module **en el propio Fire TV**.
- Ya no se conecta a la IP de Ubuntu ni requiere encender la computadora una vez instalado.
- El módulo local escucha **solo 127.0.0.1:8139 en el mismo Fire TV**, no en tu red.
- Recupera la identidad de una instalación Windows existente mediante `service.json` y `device_state.json`.
- Se compilan libcurl y Mbed TLS para la arquitectura de Vega OS con CMake y Git.
- Incluye un certificado público CA actualizado para conexiones HTTPS.

## 1. Preparación en Ubuntu

Descomprime todo el ZIP. En Windows localiza los **dos** archivos en la versión que sí reproduce:
`service.json` y `device_state.json` (de la misma instalación). Pásalos a Ubuntu y cópialos
**solo en tu ordenador** dentro de `private-config/`.

**No compartas esos JSON con nadie y nunca los envíes por un chat público.**

## 2. Instalar las herramientas que faltan

```bash
sudo apt update
sudo apt install -y build-essential cmake git python3
vega --version
```

No hace falta `libcurl4-openssl-dev` ni `libmbedtls-dev` del sistema Ubuntu:
serían bibliotecas para Ubuntu, no para Vega.

## 3. Compilar

Desde la carpeta donde está el script:

```bash
chmod +x PREPARAR-Y-COMPILAR.sh INSTALAR-FIRETV.sh
./PREPARAR-Y-COMPILAR.sh
```

Si tu SDK tiene cambios en codegen o el sistema de enlace de bibliotecas C++,
**el script puede fallar**: pásame el **primer error real** para corregirlo.
No intentes `npm install --force` ni `--legacy-peer-deps` como solución automática.

## 4. Instalar

Con el Fire TV conectado por el depurador Vega/VDA, reemplaza la IP si cambió:

```bash
./INSTALAR-FIRETV.sh 192.168.1.236:5555 x86_64
```

En tu último registro, ese mismo dispositivo aceptó un paquete `x86_64`. Si cambias a un Fire TV de arquitectura ARM, selecciona `armv7` o `aarch64` según corresponda.
Después de instalar, ya puedes apagar Ubuntu: **no hay servicios en Ubuntu**.

## 5. Seguridad

- El `.vpkg` construido **contiene** `service.json` y `device_state.json` con datos privados.
- Es un paquete **para uso personal**, no publicarlo ni distribuirlo así.
- `/pkg/assets/private/` es de solo lectura; el servicio copia estos datos a `/data/xuper-nf/`
  en el Fire TV y no vuelve a sobrescribirlos en actualizaciones.
- Si reinstalas o cambias de cuenta, podría ser necesario borrar datos de la app desde
  las opciones de Fire TV antes de importar otra instalación, conservando copia de seguridad.
- No dependas de una IP LAN ni expongas los puertos al exterior.

## 6. Qué falta probar / limitaciones

1. **No se compiló con tu Vega SDK aquí**, por lo que puede haber incompatibilidades de CMake, codegen o API.
2. **No se probaron tus credenciales reales ni tus canales** en Vega. Un mensaje de instalación exitosa no demuestra que el catálogo funcione.
3. El reproductor URL actual está enfocado en archivos/streams TS y MP4; la integración
   de HLS/DASH requiere trabajo adicional con MSE/EME y posiblemente Shaka.
4. Un CDN puede exigir cabeceras y hacer peticiones Range; el servidor local intenta
   conservarlas, pero una sesión real puede comportarse distinto.
5. Si un servidor exige verificar el dispositivo original, no hay garantía de que
   acepte la instalación Vega. No se desactivaron ni eliminaron restricciones del proveedor.

**Resumen:** fuente nativa integrada para avanzar a la versión autónoma real; no es un
`.vpkg` ya certificado como funcional.
