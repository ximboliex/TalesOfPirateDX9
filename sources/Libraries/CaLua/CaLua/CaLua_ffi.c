// Win64 uses register arguments and XMM return values. The old stack-copying
// x86 dispatcher cannot be reused. libffi implements the platform ABI.
#include "CaLua_guts.h"

#if defined(_WIN64) || defined(CLU_USE_LIBFFI)
#include <ffi.h>
#ifndef CLU_FFI_ABI
#define CLU_FFI_ABI FFI_DEFAULT_ABI
#endif

typedef struct CLUFFIType
{
    ffi_type type;
    struct CLUFFIType* next;
} CLUFFIType;

static ffi_type* CLU_FFIType(int type, CLUFFIType** owned, int depth)
{
    CLUFFIType* node;
    StructForm* form;
    int i;
    if (depth > 64) return NULL;
    if (CLU_IS_POINTER(type)) return &ffi_type_pointer;
    switch (CLU_C_BASE_TYPE(type))
    {
    case CLU_UNKNOWN: return &ffi_type_void;
    case CLU_CHAR: return CLU_IS_UNSIGNED(type) ? &ffi_type_uint8 : &ffi_type_sint8;
    case CLU_SHORT: return CLU_IS_UNSIGNED(type) ? &ffi_type_uint16 : &ffi_type_sint16;
    case CLU_INT: return CLU_IS_UNSIGNED(type) ? &ffi_type_uint32 : &ffi_type_sint32;
    case CLU_FLOAT: return &ffi_type_float;
    case CLU_DOUBLE: return &ffi_type_double;
    case CLU_STRING:
    case CLU_POINTER: return &ffi_type_pointer;
    }
    i = CLU_TYPE_TO_INDEX(type);
    if (i < 0 || i >= numStructs) return NULL;
    form = structs[i];
    if (form->numMembers <= 0) return NULL;
    node = (CLUFFIType*)calloc(1, sizeof(*node));
    if (!node) return NULL;
    node->next = *owned;
    *owned = node;
    node->type.type = FFI_TYPE_STRUCT;
    node->type.elements = (ffi_type**)calloc(form->numMembers + 1, sizeof(ffi_type*));
    if (!node->type.elements) return NULL;
    for (i = 0; i < form->numMembers; ++i)
    {
        node->type.elements[i] = CLU_FFIType(form->memberType[i], owned, depth + 1);
        if (!node->type.elements[i] || node->type.elements[i] == &ffi_type_void) return NULL;
    }
    return &node->type;
}

static void CLU_FFIPushReturn(int type, void* data)
{
    if (CLU_C_BASE_TYPE(type) == CLU_STRING)
    {
        lua_pushstring(virtualMachine, *(char**)data);
        return;
    }
    if (CLU_IS_POINTER(type))
    {
        lua_pushlightuserdata(virtualMachine, *(void**)data);
        return;
    }
    switch (CLU_C_BASE_TYPE(type))
    {
    case CLU_CHAR:
        lua_pushnumber(virtualMachine, CLU_IS_UNSIGNED(type) ? *(unsigned char*)data : *(signed char*)data);
        break;
    case CLU_SHORT:
        lua_pushnumber(virtualMachine, CLU_IS_UNSIGNED(type) ? *(unsigned short*)data : *(short*)data);
        break;
    case CLU_INT:
        if (CLU_IS_UNSIGNED(type)) lua_pushnumber(virtualMachine, *(unsigned int*)data);
        else lua_pushnumber(virtualMachine, *(int*)data);
        break;
    case CLU_FLOAT: lua_pushnumber(virtualMachine, *(float*)data); break;
    case CLU_DOUBLE: lua_pushnumber(virtualMachine, *(double*)data); break;
    case CLU_STRING: lua_pushstring(virtualMachine, *(char**)data); break;
    case CLU_POINTER: lua_pushlightuserdata(virtualMachine, *(void**)data); break;
    default: PushStructure(CLU_TYPE_TO_INDEX(type), data); break;
    }
}

int CLU_HandlerFFI(lua_State* vm)
{
    ffi_cif cif;
    ffi_type** types = NULL;
    ffi_type* returnType = NULL;
    void** values = NULL;
    void** allocations = NULL;
    void* result = NULL;
    CLUFFIType* owned = NULL;
    FuncForm* form;
    const char* error = NULL;
    int index, i, size, returns = 0;

    if (!lua_isstring(vm, 1)) return luaL_error(vm, "CLU_Call expects a registered function name");
    index = GetFunctionIndex((char*)lua_tostring(vm, 1));
    if (index < 0) return luaL_error(vm, "Unknown CaLua function");
    form = funcs[index];
    if (lua_gettop(vm) != form->numPassArgs + 1)
        return luaL_error(vm, "Wrong argument count for %s", form->name);

    PushVM(vm);
    types = (ffi_type**)calloc(form->numPassArgs ? form->numPassArgs : 1, sizeof(*types));
    values = (void**)calloc(form->numPassArgs ? form->numPassArgs : 1, sizeof(*values));
    allocations = (void**)calloc(form->numPassArgs ? form->numPassArgs : 1, sizeof(*allocations));
    if (!types || !values || !allocations) { error = "Out of memory"; goto cleanup; }
    for (i = 0; i < form->numPassArgs; ++i)
    {
        types[i] = CLU_FFIType(form->passType[i], &owned, 0);
        if (!types[i] || types[i] == &ffi_type_void) { error = "Unsupported argument type"; goto cleanup; }
        values[i] = ConvertLuaStackData(i + 2, form->passType[i], &size);
        allocations[i] = values[i];
        if (!values[i]) { error = "Cannot convert Lua argument"; goto cleanup; }
    }
    returnType = form->numRetArgs ? CLU_FFIType(form->retType, &owned, 0) : &ffi_type_void;
    // On Windows x64 both __cdecl and __stdcall use the default Win64 ABI.
    if (!returnType || ffi_prep_cif(&cif, CLU_FFI_ABI, form->numPassArgs, returnType, types) != FFI_OK)
    { error = "Cannot prepare native function ABI"; goto cleanup; }
    for (i = 0; i < form->numPassArgs; ++i)
        if (types[i]->size != (size_t)GetDataSize(form->passType[i]))
        { error = "Registered structure layout differs from native ABI"; goto cleanup; }
    if (form->numRetArgs && returnType->size != (size_t)GetDataSize(form->retType))
    { error = "Registered return layout differs from native ABI"; goto cleanup; }

    result = calloc(1, returnType->size > sizeof(ffi_arg) ? returnType->size : sizeof(ffi_arg));
    if (!result) { error = "Out of memory"; goto cleanup; }
    ffi_call(&cif, FFI_FN(form->fPtr), result, values);
    if (form->numRetArgs)
    {
        CLU_FFIPushReturn(form->retType, result);
        returns = form->numRetArgs;
    }

cleanup:
    // libffi's Win64 aggregate marshalling may rewrite avalue entries to point
    // to temporary stack copies. Free the original allocations, not avalue.
    if (allocations) for (i = 0; i < form->numPassArgs; ++i) free(allocations[i]);
    free(allocations);
    free(values);
    free(types);
    free(result);
    while (owned)
    {
        CLUFFIType* next = owned->next;
        free(owned->type.elements);
        free(owned);
        owned = next;
    }
    PopVM();
    if (error) return luaL_error(vm, "%s: %s", form->name, error);
    return returns;
}
#endif
