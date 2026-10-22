/*//\\//\\//\\//\\//\\//\\//\\//\\////\\//\\//\\//\\//\\//\\//\\//\\////\\//\\//\\//\\
  CaLua Script Binding Utility
  By Justin Reynen, 2003/2004

  Lua stack interaction routines

  If it touches the stack in any way, chances are it will be in here.
  If you're looking to hack around with CaLua, this is definitly a file you'll want to be familiar
  with.
//\\//\\//\\//\\//\\//\\//\\//\\////\\//\\//\\//\\//\\//\\//\\//\\////\\//\\//\\//\\*/

#include "CaLua_guts.h"



// Convert structure fields through the same path as function arguments so that
// signedness, pointer width and nested structures cannot disagree.
void* PopRegTable(int regStruct)
{
    StructForm* form;
    void** fields;
    void* result = NULL;
    int i, size, valid = 1;
    if (regStruct < 0 || regStruct >= numStructs || !lua_istable(virtualMachine, -1)) return NULL;
    form = structs[regStruct];
    fields = (void**)calloc(form->numMembers ? form->numMembers : 1, sizeof(*fields));
    if (!fields) return NULL;
    for (i = 0; i < form->numMembers; ++i)
    {
        lua_pushstring(virtualMachine, form->memberName[i]);
        lua_rawget(virtualMachine, -2);
        fields[i] = ConvertLuaStackData(-1, form->memberType[i], &size);
        lua_pop(virtualMachine, 1);
        if (!fields[i]) { valid = 0; break; }
    }
    if (valid) result = Pack(form->numMembers, fields, form->memberType);
    for (i = 0; i < form->numMembers; ++i) free(fields[i]);
    free(fields);
    return result;
}

void* PopValuesBySig(char* description)
{
    int count = GetNumberOfTokens(description), i, type, size, valid = 1;
    int* types;
    void** values;
    void* result = NULL;
    if (count == 0) return NULL;
    types = (int*)calloc(count, sizeof(*types));
    values = (void**)calloc(count, sizeof(*values));
    if (!types || !values) { free(types); free(values); return NULL; }
    for (i = 0; i < count; ++i)
    {
        char* token = GetToken(description, i + 1);
        type = GetDataType(token);
        free(token);
        types[i] = type;
        size = 0;
        values[i] = ConvertLuaStackData(-count + i, type, &size);
        if (!values[i]) { valid = 0; break; }
    }
    if (valid) result = Pack(count, values, types);
    for (i = 0; i < count; ++i) free(values[i]);
    free(values);
    free(types);
    lua_pop(virtualMachine, count);
    return result;
}

int PushStructure(int regStruct, void* data)
{
    StructForm* form;
    int i, base = lua_gettop(virtualMachine), table;
    char* cursor = (char*)data;
    if (!data) { lua_pushnil(virtualMachine); return 0; }
    if (regStruct < 0 || regStruct >= numStructs || !lua_checkstack(virtualMachine, 3)) return 1;
    form = structs[regStruct];
    lua_newtable(virtualMachine);
    table = lua_gettop(virtualMachine);
    for (i = 0; i < form->numMembers; ++i)
    {
        lua_pushstring(virtualMachine, form->memberName[i]);
        if (CToLuaData(form->memberType[i], cursor))
        { lua_settop(virtualMachine, base); return 1; }
        lua_settable(virtualMachine, table);
        cursor += form->NextMemberOffset[i];
    }
    return 0;
}

void* ConvertLuaStackData(int position, int type, int* size)
{
    int kind = CLU_C_BASE_TYPE(type), luaType = lua_type(virtualMachine, position);
    void* result;
    lua_Number number = 0;
    *size = GetDataSize(type);
    if (*size <= 0) return NULL;
    if ((CLU_IS_POINTER(type) && kind != CLU_STRING) || kind == CLU_POINTER)
    {
        if (luaType != LUA_TNIL && luaType != LUA_TUSERDATA && luaType != LUA_TLIGHTUSERDATA) return NULL;
        result = CLU_Calloc(sizeof(void*));
        if (result) *(void**)result = lua_touserdata(virtualMachine, position);
        return result;
    }
    if (kind == CLU_STRING)
    {
        if (luaType != LUA_TNIL && luaType != LUA_TSTRING && luaType != LUA_TUSERDATA && luaType != LUA_TLIGHTUSERDATA) return NULL;
        result = CLU_Calloc(sizeof(char*));
        if (result) *(const void**)result = luaType == LUA_TSTRING ? (const void*)lua_tostring(virtualMachine, position) : lua_touserdata(virtualMachine, position);
        return result;
    }
    if (kind > CLU_NUM_C_TYPES)
    {
        if (!lua_istable(virtualMachine, position)) return NULL;
        lua_pushvalue(virtualMachine, position);
        result = PopRegTable(CLU_TYPE_TO_INDEX(type));
        lua_pop(virtualMachine, 1);
        return result;
    }
    if (luaType == LUA_TNUMBER) number = lua_tonumber(virtualMachine, position);
    else if (luaType == LUA_TBOOLEAN) number = lua_toboolean(virtualMachine, position);
    else if (kind == CLU_CHAR && luaType == LUA_TSTRING)
        number = (unsigned char)*lua_tostring(virtualMachine, position);
    else if (luaType != LUA_TNIL) return NULL;

    // Keep the legacy bit patterns for negative unsigned values, and unsigned
    // byte/short inputs, without undefined floating-to-integer conversions.
    if (kind == CLU_CHAR && !(number >= -128 && number <= 255)) return NULL;
    if (kind == CLU_SHORT && !(number >= -32768 && number <= 65535)) return NULL;
    if (kind == CLU_INT && !(number >= -2147483648.0 && number <= 4294967295.0)) return NULL;
    result = CLU_Calloc(*size);
    if (!result) return NULL;
    switch (kind)
    {
    case CLU_CHAR: *(unsigned char*)result = number < 0 ? (unsigned char)(int)number : (unsigned char)number; break;
    case CLU_SHORT: *(unsigned short*)result = number < 0 ? (unsigned short)(int)number : (unsigned short)number; break;
    case CLU_INT: *(unsigned int*)result = number < 0 ? (unsigned int)(int)number : (unsigned int)number; break;
    case CLU_FLOAT: *(float*)result = (float)number; break;
    case CLU_DOUBLE: *(double*)result = number; break;
    default: free(result); return NULL;
    }
    return result;
}

int CToLuaData(int type, void* data)
{
    int kind = CLU_C_BASE_TYPE(type);
    if (CLU_IS_POINTER(type) && kind != CLU_STRING && kind <= CLU_NUM_C_TYPES)
    { lua_pushlightuserdata(virtualMachine, *(void**)data); return 0; }
    switch (kind)
    {
    case CLU_CHAR: lua_pushnumber(virtualMachine, CLU_IS_UNSIGNED(type) ? *(unsigned char*)data : *(signed char*)data); break;
    case CLU_SHORT: lua_pushnumber(virtualMachine, CLU_IS_UNSIGNED(type) ? *(unsigned short*)data : *(short*)data); break;
    case CLU_INT:
        if (CLU_IS_UNSIGNED(type)) lua_pushnumber(virtualMachine, *(unsigned int*)data);
        else lua_pushnumber(virtualMachine, *(int*)data);
        break;
    case CLU_FLOAT: lua_pushnumber(virtualMachine, *(float*)data); break;
    case CLU_DOUBLE: lua_pushnumber(virtualMachine, *(double*)data); break;
    case CLU_STRING: lua_pushstring(virtualMachine, *(const char**)data); break;
    case CLU_POINTER: lua_pushlightuserdata(virtualMachine, *(void**)data); break;
    case CLU_UNKNOWN: CLU_Error("Cannot push unknown native type"); return 1;
    default: return PushStructure(CLU_TYPE_TO_INDEX(type), CLU_IS_POINTER(type) ? *(void**)data : data);
    }
    return 0;
}
