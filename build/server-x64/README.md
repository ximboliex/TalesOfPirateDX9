# Migracion del servidor a x64 — estado de trabajo

**No es una migracion terminada ni una entrega lista para produccion.**
La rama parte del lote `codex/mob-runtime-p0` (`e07dcd0`). No se ha compilado
ninguno de los cuatro ejecutables de Windows en este entorno Linux.

## Cobertura implementada

| Area | Cambio | Evidencia disponible |
| --- | --- | --- |
| Compilacion | `sources/server-x64.sln`: cuatro servidores y ocho bibliotecas, Debug/Release reales x64; Common usa Long_Struct_Debug/Release | Comprobacion XML de 12 proyectos y 24 correspondencias, sin alias a Win32 |
| Aislamiento | Binarios y bibliotecas en `sources/out/server/x64/<config>`; objetos por proyecto; busqueda de bibliotecas sin directorios x86 | Revision de propiedades compartidas |
| Crypto++ | Usa su fuente y ensamblador x64 existentes; selecciona v143 y salida comun | Pendiente MSBuild |
| Referencias remotas | `ToAddress` exporta identificadores tipados en x64, no direcciones; `ToPointer` consulta un registro local | Direcciones reales >4 GB, tipos incorrectos, referencias caducadas, reciclaje y concurrencia |
| Ciclo de vida | ClientConnection, Player y CPlayer invalidan identificadores en Initially, Finally y destructor | Pruebas del registro; callbacks reales pendientes de integracion Windows |
| SQL de presencia | Conserva `endeMem` y sus campos INT de 32 bits; exige que ZeroAddr termine correctamente antes de iniciar | Revision de productores/consumidores y controles nulos |
| Red | Conserva los campos de cuatro bytes y las colas/trailers existentes; corrige el orden de ReverseReadLongLong | Prueba de palabras alta/baja contra los metodos extraidos de Packet.cpp |
| Cifrado | Sustituye tres funciones naked x86 por C++; conserva incluso la comparacion signed de CL y el enmascarado de rotaciones | 65.536 entradas de rotacion, 65.536 combinaciones XOR y 100 longitudes de encrypt_B |
| ODBC | Indicadores de longitud SQLLEN, precision SQLULEN; no amplia ids ni estadisticas del juego | Revision de las firmas; integracion con SQL pendiente |
| IOCP | Usa ULONG_PTR para claves de completado | Revision estatica; transporte alternativo no habilitado en la solucion nueva |
| Diagnostico | RuntimeStack usa CONTEXT/STACKFRAME64 y registros AMD64; registra direcciones completas | Compilacion y prueba de excepciones pendientes; esta ruta x64 no resuelve nombres/lineas |
| Windows minimo | SDK define Windows 7 como minimo para GetTickCount64 ya usado por P0 | Pendiente compilacion Windows |

## Bloqueos concretos

1. **BTI / AccountServer:** `Libraries/Ipss/include/BTI.h` expone
   `ipBTI_Initial(LONG HWnd)`. `AccountServer/src/BillService.cpp` convierte HWND
   a LONG. El repositorio no contiene la implementacion ni un SDK x64. No se
   puede asumir que cambiar el cast o renombrar una DLL conserve el ABI. Hace
   falta un SDK del proveedor con contrato compatible con x64, o implementar y
   validar un puente a un proceso x86 para las llamadas y callbacks BTI. El
   script rechaza All/AccountServer explicitamente. No deshabilita billing.
2. **ICU:** hay cabeceras ICU 3.8 y DLL de runtime x86, pero faltan las fuentes
   y bibliotecas de enlace x64. Proporcionar/recompilar ICU 3.8 para x64 con sus
   DLL y recursos coincidentes. Actualizar a ICU moderno seria otra migracion:
   incluye ABI, namespaces, datos `.res` y pruebas de traducciones/formato.
3. **Validacion Windows:** faltan MSVC/Windows SDK/ODBC y un servidor SQL de
   pruebas en este entorno. Las pruebas portables no validan el ABI LLP64 de
   Windows ni el enlace de todos los proyectos.

## Identificadores y sus limites

El registro se activa con `_WIN64`; las compilaciones Win32 conservan el
comportamiento previo de direcciones. Los servidores pares solo deben tratar
los identificadores recibidos como valores opacos y devolverlos a su propietario.
CPlayer es el tipo canonico incluso al exportar una lista GatePlayer.

Cero significa referencia nula. Los identificadores son positivos y caben en
SQL INT. No se reutilizan dentro del proceso: agotar 2.147.483.647 exportaciones
unicas provoca una excepcion explicita, nunca un wrap que reactive paquetes
viejos. El registro se conserva hasta terminar el proceso para que los
destructores estaticos puedan invalidar referencias. Solo conserva las entradas
activas; no mantiene un historial de objetos liberados.

La exclusion mutua protege el registro, **no prolonga la vida del objeto despues
de Resolve**. Las rutas de uso deben conservar la sincronizacion de sus objetos.
Esta migracion no demuestra que todas las carreras historicas de logout,
desconexion y combate hayan desaparecido. Es obligatorio verificar esas rutas
en el servidor real antes de desplegar. Reiniciar coordinadamente los servicios;
no restaurar en otra ejecucion identificadores de una sesion anterior. GroupServer
limpia la presencia SQL antes de aceptar sesiones.

## Compilacion prevista en Windows

Usar VS 2022, herramientas C++ x64 v143, Windows SDK y driver ODBC **x64**.
Los proyectos usan CRT estatico /MT en Release y /MTd en Debug.

Preparar en `dependencies/server-x64/Release/lib`:
`icudt.lib`, `icuuc.lib`, `icuin.lib`; en `Debug/lib`:
`icudt.lib`, `icuucd.lib`, `icuind.lib`. Deben ser bibliotecas AMD64 que
correspondan a las cabeceras ICU 3.8 suministradas. Sus DLL deben estar presentes
en el entorno de ejecucion y verificarse igualmente como AMD64.

En una consola **x64 Native Tools for VS 2022**, desde la raiz:

```powershell
powershell -File build/server-x64/Build.ps1 -Target GameServer -Configuration Release -BuildLua
powershell -File build/server-x64/Build.ps1 -Target GroupServer -Configuration Release -BuildLua
powershell -File build/server-x64/Build.ps1 -Target GateServer -Configuration Release
```

`-BuildLua` construye el LuaJIT incluido en una copia aislada, sin reutilizar
`src/lua51.lib` del repositorio. El script comprueba la arquitectura de las
bibliotecas requeridas y del ejecutable con dumpbin y genera un `.binlog`.
Repetir para Debug. El script aun no se ha ejecutado en PowerShell/MSVC.
La solucion principal del cliente conserva sus correspondencias existentes;
para el servidor se debe usar la solucion nueva, no sus antiguos alias x64.
`IOCP_Release` es un transporte alternativo historico y no se declara migrado.

Prueba independiente del ABI, sin ICU, BTI ni SQL en ejecucion:

```bat
cl /nologo /EHsc /std:c++17 /I sources\Libraries\Server\sdk\include tests\server_x64\windows_abi.cpp /Fe:sources\out\server\windows_abi.exe
sources\out\server\windows_abi.exe
```

Crear primero el directorio de salida. Esta prueba tambien esta pendiente.

## Pruebas ejecutadas aqui

```bash
python tests/server_x64/run_tests.py
python tests/mob_runtime/run_tests.py
```

Ambas pasan con g++ de 64 bits y AddressSanitizer/UndefinedBehaviorSanitizer.
LeakSanitizer se desactiva por las restricciones de `/proc` del contenedor.
La suite x64 usa la clase de registro de produccion y funciones extraidas de
algo.cpp/Packet.cpp. Para Packet sustituye el almacenamiento por un vector de
palabras: comprueba el orden, no toda la pila de red ni su framing.
P0 conserva 50 escenarios, 10.000 ciclos temporales y 78.141 comparaciones
geometricas. No son pruebas de arranque de los servidores.

## Criterios para terminar la migracion

- Resolver BTI sin suprimir autenticacion, billing ni sus callbacks.
- Compilar y enlazar los cuatro ejecutables AMD64 en Debug y Release y revisar
  todos los diagnosticos de truncamiento; verificar dependencias de cada EXE.
- Validar recursos ICU y LuaJIT, inicializacion, ODBC, insercion/lectura/NULL de
  columnas y datos guardados con la version de 32 bits.
- Conectar el cliente actual: login/logout, seleccion/borrado/creacion de personaje,
  entrar/salir/cambiar mapa, inventario, comercio, amigos, gremios y desconexiones.
- Probar reciclaje masivo de conexiones/jugadores, paquetes retrasados, caida y
  reconexion de Game/Gate/Group, y reinicio con presencia SQL antigua.
- Verificar representaciones binarias persistidas y estructuras copiadas a paquetes;
  compilar x64 no demuestra compatibilidad de cada formato.
- Probar mobs/combate/rutas/aggro y comparar tiempos de tick y memoria bajo carga.
- Validar excepciones y trazas, ejecutar prueba prolongada y conservar rollback
  de ejecutables/configuracion/datos antes de habilitar produccion.

Referencias de ABI: [ODBC 64-bit (Microsoft)](https://learn.microsoft.com/en-us/sql/odbc/reference/odbc-64-bit-information)
y [StackWalk (Microsoft)](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk).
