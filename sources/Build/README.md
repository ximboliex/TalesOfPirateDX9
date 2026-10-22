# Cliente MindPower: DirectX 9 y Windows x64

## Estado de esta migración

La solución `sources/ClientDX9.sln` reúne el cliente, MindPower y sus once
dependencias internas con configuraciones Debug/Release x64 reales. Los proyectos
del cliente y del motor usan DX9 en todas sus configuraciones. En la solución
general, las configuraciones x64 compilan ese mismo conjunto; los servidores
siguen siendo Win32 y no se compilan al seleccionar x64. Las soluciones antiguas
que referencian archivos `.vcproj` no son el punto de entrada de esta migración.

Se sustituyó el ensamblador x86 activo, se corrigieron direcciones y callbacks que
se truncaban a 32 bits y se añadió un dispatcher libffi a CaLua para Windows x64.
Lua conserva instrucciones y longitudes de cadenas de chunks de 32 bits al
escribir, y acepta longitudes de 32 o 64 bits al leer. Los campos DWORD de los
modelos no se convierten a punteros de 64 bits. Los registros de texturas
LGO/LMO se mantienen en 208 bytes y descartan direcciones de memoria antiguas
al cargar; la versión anterior mantiene sus registros de 240 bytes. Se añadieron
lecturas acotadas y propagación de errores en materiales, mallas y helpers,
lectura de offsets superiores a 2 GiB y carga de contenedores LMO con reemplazo
del modelo únicamente al completar la lectura.

Los shaders de cielo y de materiales ya usan punteros COM del tamaño correcto,
con liberación y referencias al clonar. Los efectos DX9 conservan la técnica
inicial `t0`, envían los parámetros DWORD y equilibran las llamadas
Begin/BeginPass/EndPass/End incluso cuando falla CommitChanges o EndPass.
También se corrigen el diagnóstico de trazas AMD64 y el registro de direcciones
y tamaños del asignador de ICUHelper. Estos cambios no añaden todavía nuevos
lectores de efectos ni reproducción automática de animaciones de armas.

**No hay una compilación completa de Windows validada todavía.** El entorno de
trabajo usado es Linux, sin MSVC ni Windows SDK. El repositorio tampoco incluye
las bibliotecas externas AMD64. Las comprobaciones estáticas y las pruebas
portables descritas abajo no sustituyen compilar, enlazar y ejecutar el juego.

## Dependencias para compilar

Instalar Visual Studio 2022 con C++ v143, Windows SDK y ATL. Instalar Python 3.
Para D3DX9, usar las cabeceras y `Lib/x64/d3dx9.lib` del DirectX SDK June 2010;
el Windows SDK moderno por sí solo no proporciona D3DX9. Se puede indicar otro
directorio compatible explícitamente. DirectInput sigue usando su API 8, como
corresponde a DX9; eso no implica un renderer Direct3D 8.

Preparar `sources/Dependencies/x64/` así:

```text
include/ffi.h
include/ffitarget.h
lib/Release/SDL.lib
lib/Release/SDLmain.lib
lib/Release/SDL_mixer.lib
lib/Release/discord-rpc.lib
lib/Release/icudt.lib
lib/Release/icuuc.lib
lib/Release/icuin.lib
lib/Release/libffi.lib
lib/Debug/…
```

Se requieren SDL 1.2 y SDL_mixer de la misma API, Discord RPC compatible con las
cabeceras incluidas, e ICU compatible con las cabeceras ICU 3.8 de este árbol.
No basta con renombrar bibliotecas de SDL2 o de una versión reciente de ICU.
Debug utiliza `icuucd.lib` e `icuind.lib`; el resto de nombres coincide con
Release. Compilar libffi con soporte Windows AMD64, incluidas las estructuras
por valor. Mantener el runtime estático `/MT` en Release y `/MTd` en Debug para
las dependencias estáticas que intercambien memoria con el cliente.

Para preparar libffi desde una receta y fuentes fijadas, en Windows:

```powershell
./sources/Build/Prepare-LibffiX64.ps1
```

El script crea un checkout aislado de vcpkg bajo `Build/out`, compila libffi con
CRT estático y copia ambas configuraciones y sus cabeceras. No instala las otras
dependencias. La integración define `FFI_BUILDING`/`FFI_STATIC_BUILD` para evitar
imports de DLL al enlazar esta biblioteca estática. Si se proporciona libffi
como DLL, compilar con `/p:LibffiStatic=false` y suministrar su biblioteca de
importación y DLL AMD64 correspondientes.

Las DLL necesarias deben ser también AMD64 y corresponder a esas bibliotecas de
importación. Las DLL existentes en `Client/system` son x86 y no sirven para el
nuevo cliente. El preflight inspecciona la arquitectura real de cada `.lib`;
tener una carpeta llamada x64 no basta.

## Compilación en Windows

Desde PowerShell, con `DXSDK_DIR` apuntando al SDK:

```powershell
./sources/Build/Build-ClientDX9.ps1 -Configuration Release
```

Rutas explícitas si las dependencias están fuera del repositorio:

```powershell
./sources/Build/Build-ClientDX9.ps1 -Configuration Debug `
  -DependenciesRoot 'D:\Dependencies\x64' `
  -D3DXIncludeDir 'D:\DXSDK\Include' `
  -D3DXLibraryDir 'D:\DXSDK\Lib\x64'
```

El script ejecuta el preflight, compila los 13 proyectos y verifica que Game,
MindPower y CaLua resultantes son AMD64. Los binarios quedan aislados en
`sources/Build/out/bin/x64/{Debug,Release}`; bibliotecas y objetos tienen sus
propios directorios. El log MSBuild se guarda en `sources/Build/out`.
El script no sobrescribe la instalación del juego.

Antes de validar el juego completo, ejecutar también desde **x64 Native Tools
PowerShell de Visual Studio 2022**:

```powershell
./sources/Build/Test-ClientX64.ps1 -Configuration Release
./sources/Build/Test-ClientX64.ps1 -Configuration Debug
```

Estas pruebas compilan los tipos reales de assets de MindPower para comprobar su
layout LLP64, ejecutan la captura de trazas AMD64 y prueban Lua/CaLua con el ABI
nativo de MSVC, incluida la ruta C→Lua de estructuras en varargs. Solo necesitan
las cabeceras DX9 incluidas y libffi; no requieren las bibliotecas del cliente
completo. El workflow `Client x64 ABI regressions` contiene ambas configuraciones
nativas y las pruebas Linux. El estado de ejecución nativa debe consultarse en GitHub Actions; el entorno
local Linux no puede compilar ni ejecutar MSVC ni Direct3D.

Para probar, usar una copia del cliente con sus assets y configuración; colocar
allí los nuevos binarios y todas sus DLL AMD64, y ejecutar desde el directorio
que espera el cliente para sus rutas relativas. Verificar inicio, conexión,
scripts `.clu`, UI, audio, carga de personajes/mapas, efectos, cambio de
resolución, pérdida/restauración del dispositivo y cierre. Comparar las escenas
con el cliente anterior. Los avisos de truncamiento de punteros son errores de
compilación en x64: resolverlos conservando los formatos de disco y de red.

## Comprobaciones portables

```sh
python sources/Build/check_client_x64.py --projects-only
python -m unittest discover -s sources/Build/tests -p 'test_*.py'
python sources/Build/run_portability_tests.py \
  --ffi-include /ruta/a/cabeceras/libffi \
  --ffi-library /ruta/a/libffi.so
```

La última prueba requiere Linux x86_64, GCC/G++ y libffi con FFI_WIN64. Compila
39 unidades de Lua/CaLua con ASan/UBSan y usa funciones GCC `ms_abi` para
ejercitar la convención Windows x64: argumentos mixtos, argumentos en pila,
retornos, punteros superiores a 32 bits y estructuras por valor/anidadas. Incluye
punteros tipados, límites y campos sin signo, errores repetidos sin crecimiento
de la pila Lua y chunks Lua heredados.
También comprueba chunks Lua, la conversión entre registros de texturas de
208 bytes y estructuras con punteros de 64 bits, y todas las combinaciones de
rotación/XOR de bytes. Se ejercitan lecturas truncadas, offsets reales superiores
a 2 GiB, limpieza del asignador al fallar un constructor y recuperación de pases
de efectos. ASan/UBSan y el detector de fugas están activados por defecto; sus
resultados se limitan a estas pruebas y no ejercitan el renderer. La ruta C→Lua
de varargs con estructuras todavía requiere ejecutar la prueba nativa en MSVC.

Para comprobar los assets existentes y probar su conversión real:

```sh
python sources/Build/audit_model_assets.py Client/model \
  --json /tmp/model-boundaries.json --texture-records /tmp/model-textures.bin
python sources/Build/run_portability_tests.py \
  --ffi-include /ruta/a/cabeceras/libffi --ffi-library /ruta/a/libffi.so \
  --texture-records /tmp/model-textures.bin
```

El inventario de esta revisión encontró 7.389 modelos: 7.278 pasaron la
comprobación de límites de geometría/materiales y proporcionaron 46.132 registros
de texturas, todos convertidos de ida y vuelta correctamente. Otros 111 archivos
requieren revisión: hay archivos vacíos, versiones desconocidas y tamaños
declarados inconsistentes. El auditor devuelve un código de error si los
encuentra. No modifica los assets ni demuestra que sus mallas/animaciones sean
correctas; tampoco clasifica automáticamente una variante desconocida como
archivo corrupto.

Para comprobar una DLL o EXE adicional:

```sh
python sources/Build/check_client_x64.py --projects-only --binary /ruta/a/archivo.dll
```
