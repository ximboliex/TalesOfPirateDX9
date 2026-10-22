// Build with the real CaLua/Lua sources and libffi. On Linux, define
// CLU_TEST_MS_ABI and CLU_FFI_ABI=FFI_WIN64 to exercise the Windows x64 ABI.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CaLua.h"

#ifdef CLU_TEST_MS_ABI
#define NATIVE_ABI __attribute__((ms_abi))
#else
#define NATIVE_ABI
#endif

typedef struct Payload { double x; int count; void* pointer; } Payload;
typedef struct Nested { char tag; Payload data; short tail; } Nested;
typedef struct Scalars { unsigned char byte; unsigned short word; unsigned int value; int* address; } Scalars;
typedef struct ScriptResult { unsigned int value; void* pointer; Payload data; } ScriptResult;

static double NATIVE_ABI Mixed(int a, float b, double c, void* p, int d, double e)
{
    assert(p != NULL);
    return a + b + c + d + e;
}
static float NATIVE_ABI FloatValue(void) { return 1.25f; }
static unsigned int NATIVE_ABI UnsignedValue(void) { return UINT32_MAX; }
static void* NATIVE_ABI Identity(void* p) { return p; }
static const char* NATIVE_ABI Text(void) { return "native string"; }
static Payload NATIVE_ABI MakePayload(void* p)
{
    Payload result = { 2.5, 7, p };
    return result;
}
static double NATIVE_ABI ReadPayload(Payload p) { return p.x + p.count; }
static int NATIVE_ABI ReadNested(Nested n) { return n.tag + n.data.count + n.tail; }
static unsigned int NATIVE_ABI EchoUnsigned(unsigned int n) { return n; }
static int* NATIVE_ABI TypedPointer(int* p) { return p; }
static Scalars NATIVE_ABI EchoScalars(Scalars value) { return value; }
static unsigned int NATIVE_ABI CheckScalars(Scalars value)
{
    assert(value.byte == 255 && value.word == 65535 && value.address != NULL);
    return value.value;
}

static void Run(lua_State* L, const char* source)
{
    int status = luaL_loadbuffer(L, source, strlen(source), "abi-test");
    if (!status) status = lua_pcall(L, 0, 0, 0);
    if (status) fprintf(stderr, "%s\n", lua_tostring(L, -1));
    assert(status == 0);
}

static unsigned char chunk[8192];
static size_t chunk_size;
static int Writer(lua_State* L, const void* data, size_t size, void* unused)
{
    (void)L; (void)unused;
    assert(chunk_size + size <= sizeof(chunk));
    memcpy(chunk + chunk_size, data, size);
    chunk_size += size;
    return 0;
}

int main(void)
{
    lua_State* L;
    void* pointer = malloc(32);
    assert(sizeof(void*) == 8);
    assert((uintptr_t)pointer > UINT32_MAX);
    assert(CLU_Init() >= 0);
    L = CLU_GetVirtualMachine();
    assert(CLU_RegisterStructure("Payload", "double:x, int:count, ptr:pointer") == 0);
    assert(CLU_RegisterStructure("Nested", "char:tag, Payload:data, short:tail") == 0);
    assert(CLU_RegisterStructure("Scalars", "uchar:byte, ushort:word, uint:value, int*:address") == 0);
#define REGISTER(name, ret, args) assert(CLU_RegisterFunction(#name, ret, args, CLU_CDECL, CLU_CAST(name)) == 0)
    REGISTER(Mixed, "double", "int,float,double,ptr,int,double");
    REGISTER(FloatValue, "float", NULL);
    REGISTER(UnsignedValue, "uint", NULL);
    REGISTER(Identity, "ptr", "ptr");
    REGISTER(Text, "char*", NULL);
    REGISTER(MakePayload, "Payload", "ptr");
    REGISTER(ReadPayload, "double", "Payload");
    REGISTER(ReadNested, "int", "Nested");
    REGISTER(EchoUnsigned, "uint", "uint");
    REGISTER(TypedPointer, "int*", "int*");
    REGISTER(EchoScalars, "Scalars", "Scalars");
    REGISTER(CheckScalars, "uint", "Scalars");
    lua_pushlightuserdata(L, pointer);
    lua_setglobal(L, "native_pointer");
    Run(L,
        "assert(CLU_Call('Mixed',1,2.5,3.25,native_pointer,4,5.5)==16.25)\n"
        "assert(CLU_Call('FloatValue')==1.25)\n"
        "assert(CLU_Call('UnsignedValue')==4294967295)\n"
        "assert(CLU_Call('Identity',native_pointer)==native_pointer)\n"
        "assert(CLU_Call('Text')=='native string')\n"
        "p=CLU_Call('MakePayload',native_pointer)\n"
        "assert(p.x==2.5 and p.count==7 and p.pointer==native_pointer)\n"
        "assert(CLU_Call('ReadPayload',p)==9.5)\n"
        "assert(CLU_Call('ReadNested',{tag=2,data=p,tail=4})==13)\n"
        "assert(CLU_Call('EchoUnsigned',4294967295)==4294967295)\n"
        "assert(CLU_Call('EchoUnsigned',-1)==4294967295)\n"
        "assert(CLU_Call('TypedPointer',native_pointer)==native_pointer)\n"
        "s=CLU_Call('EchoScalars',{byte=255,word=65535,value=4294967295,address=native_pointer})\n"
        "assert(s.byte==255 and s.word==65535 and s.value==4294967295 and s.address==native_pointer)\n"
        "assert(CLU_Call('CheckScalars',s)==4294967295)\n"
        "assert(not pcall(CLU_Call,'Mixed',1))\n"
        "assert(not pcall(CLU_Call,'TypedPointer',123))\n"
        "assert(not pcall(CLU_Call,'EchoUnsigned',4294967296))\n"
        "assert(not pcall(CLU_Call,'ReadPayload',{x='invalid',count=7,pointer=native_pointer}))\n"
        "assert(not pcall(CLU_Call,'missing'))\n"
        "function return_values(n,p) return n,p,{x=2.5,count=7,pointer=p} end\n"
        "function script_error() error('expected error') end\n"
        "function aggregate_arg(p) assert(p.x==2.5 and p.count==7) return p end\n");
    {
        int baseline = lua_gettop(L), i;
        ScriptResult* result = (ScriptResult*)CLU_CallScriptFunction("return_values", "uint,ptr,Payload", "uint,ptr", UINT32_MAX, pointer);
        assert(result && result->value == UINT32_MAX && result->pointer == pointer);
        assert(result->data.count == 7 && result->data.pointer == pointer);
        CLU_DllFree(result);
        for (i = 0; i < 20; ++i)
        {
            assert(CLU_CallScriptFunction("missing", NULL, NULL) == NULL);
            assert(CLU_CallScriptFunction("script_error", NULL, NULL) == NULL);
            assert(CLU_CallScriptFunction("return_values", NULL, "bad_type", 0) == NULL);
            assert(lua_gettop(L) == baseline);
        }
    }
#ifdef _WIN64
    {
        Payload argument = {2.5,7,pointer};
        Payload* result = (Payload*)CLU_CallScriptFunction("aggregate_arg", "Payload", "Payload", argument);
        assert(result && result->pointer == pointer && result->count == 7);
        CLU_DllFree(result);
    }
#endif

    // The x64 VM continues to write legacy 32-bit size_t fields in chunks.
    assert(luaL_loadbuffer(L, "return 'legacy chunk',42", strlen("return 'legacy chunk',42"), "chunk") == 0);
    assert(lua_dump(L, Writer, NULL) == 1);
    assert(chunk[7] == 4 && chunk[8] == 4); // signature, version, endian, int width, size_t width
    lua_pop(L, 1);
    assert(luaL_loadbuffer(L, (const char*)chunk, chunk_size, "roundtrip") == 0);
    assert(lua_pcall(L, 0, 2, 0) == 0);
    assert(strcmp(lua_tostring(L, -2), "legacy chunk") == 0);
    assert(lua_tonumber(L, -1) == 42);
    CLU_Shutdown();
    free(pointer);
    puts("CaLua Win64 ABI and legacy chunk roundtrip passed");
    return 0;
}
