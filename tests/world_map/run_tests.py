#!/usr/bin/env python3
"""Portable production core + complete service with game/Win32 doubles; not a game build."""
from pathlib import Path
import os, subprocess, tempfile, re, xml.etree.ElementTree as ET, shutil, ctypes, ctypes.util
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
COMMON=ROOT/'sources/Libraries/common/include'
SERVER=ROOT/'sources/Server/GameServer/src'
def compile_run(source, output, cwd):
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I',str(COMMON),'-I',str(SERVER),str(source),'-o',str(output)],check=True)
 subprocess.run([str(output)],cwd=cwd,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
with tempfile.TemporaryDirectory(prefix='world-map-') as d:
 tmp=Path(d);(tmp/'resource').mkdir()
 config=ROOT/'server/GameServer/resource'
 (tmp/'resource/map_poi_types.txt').write_bytes((config/'map_poi_types.txt').read_bytes()+b'999 2 2000\n')
 shutil.copy(config/'map_poi_worlds.txt',tmp/'resource')
 compile_run(HERE/'core.cpp',tmp/'core',tmp)
 service=(SERVER/'MapPOIService.cpp').read_text()
 service=re.sub(r'^#include.*\n','',service,flags=re.M)
 generated=tmp/'service.cpp'
 generated.write_text((HERE/'service_env.hpp').read_text()+service+(HERE/'service_cases.cpp').read_text())
 compile_run(generated,tmp/'service',tmp)
 # Compile actual client tile cache/update/show methods with texture/scene doubles.
 client=(ROOT/'sources/Client/src/SMallMap.cpp').read_bytes().decode('latin1')
 fragment=client[client.index('void\tCLargerMap::Show'):client.index('void\tCSMNpc::Render')]
 generated=tmp/'tiles.cpp'
 generated.write_text((HERE/'tiles_env.hpp').read_text()+fragment+(HERE/'tiles_cases.cpp').read_text())
 compile_run(generated,tmp/'tiles',tmp)
 # Runtime form encryption is the game's fixed +23 byte transform.
 form=ROOT/'Client/scripts/lua/forms/main.clu'
 assert bytes((b+23)&255 for b in form.read_bytes())==form.with_suffix('.bin').read_bytes()
 commands=(COMMON/'NetCommand.h').read_bytes().decode('latin1')
 defines={a:b.strip().split('//')[0] for a,b in re.findall(r'^#define\s+(CMD_\w+)\s+([^\r\n]+)',commands,re.M)}
 def value(name,stack=()):
  if name in stack:raise ValueError(name)
  expr=defines[name]
  expr=re.sub(r'CMD_\w+',lambda m:str(value(m[0],stack+(name,))),expr)
  if not re.fullmatch(r'[\s\d+xXa-fA-F()*/+-]+',expr):raise ValueError(name)
  return eval(expr,{'__builtins__':{}},{})
 for target in ('CMD_CM_MAP_POI','CMD_MC_MAP_POI'):
  v=value(target)
  for name in defines:
   if name==target or name.endswith('_BASE'):continue
   try:other=value(name)
   except (ValueError,SyntaxError,NameError):continue
   assert v!=other,(target,name,v)
 ns={'p':'http://schemas.microsoft.com/developer/msbuild/2003'}
 project=ET.parse(ROOT/'sources/Server/GameServer/Proj/GameServer.vcxproj')
 assert any(e.get('Include','').endswith('MapPOIService.cpp') for e in project.findall('.//p:ClCompile',ns))
 print('PASS: form binary parity, unique packet IDs, project source inclusion')

# Parse without executing game globals; this is not a LuaJIT runtime test.
library=ctypes.util.find_library('lua5.4')
if library:
 lua=ctypes.CDLL(library);lua.luaL_newstate.restype=ctypes.c_void_p
 lua.luaL_loadbufferx.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,ctypes.c_char_p]
 lua.lua_tolstring.argtypes=[ctypes.c_void_p,ctypes.c_int,ctypes.c_void_p];lua.lua_tolstring.restype=ctypes.c_char_p
 lua.lua_close.argtypes=[ctypes.c_void_p]
 for path in [ROOT/'Client/scripts/lua/forms/main.clu',*config.glob('*/entry.lua')]:
  data=path.read_bytes();state=lua.luaL_newstate()
  result=lua.luaL_loadbufferx(state,data,len(data),str(path).encode(),b't')
  if result:raise AssertionError((path,lua.lua_tolstring(state,-1,None)))
  lua.lua_close(state)
 print('PASS: Lua syntax (Lua 5.4 parser; not GUI execution)')
else:print('SKIP: Lua syntax parser unavailable')
