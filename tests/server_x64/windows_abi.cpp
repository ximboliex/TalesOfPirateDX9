// Compile AND run in VS 2022 x64 Native Tools, independently of external libraries:
// cl /nologo /EHsc /std:c++17 /I sources\Libraries\Server\sdk\include tests\server_x64\windows_abi.cpp
#include "DBCCommon.h"
#include <sql.h>
#include <sqlext.h>
#include <cassert>
#include <type_traits>
static_assert(sizeof(void*) == 8, "x64 required");
static_assert(sizeof(dbc::uLong) == 4, "Packet long must remain 32 bits");
static_assert(sizeof(dbc::Long) == 4, "SQL/protocol long must remain 32 bits");
static_assert(sizeof(SQLLEN) == 8 && sizeof(SQLULEN) == 8, "ODBC x64 ABI");
static_assert(sizeof(ULONG_PTR) == 8, "IOCP x64 ABI");
static_assert(sizeof(decltype(dbc::ToAddress(static_cast<int*>(nullptr)))) == 4, "Wire handle width");
int main() {
    int object = 42;
    const auto handle = dbc::ToAddress(&object);
    assert(handle && dbc::ToPointer<int>(handle) == &object);
    assert(dbc::ToPointer<double>(handle) == nullptr);
    dbc::ForgetAddress(&object);
    assert(dbc::ToPointer<int>(handle) == nullptr);
    assert(dbc::ToAddress(&object) != handle);
    dbc::ForgetAddress(&object);
}
