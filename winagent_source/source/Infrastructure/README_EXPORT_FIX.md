# ResourceMonitor Export Fix

## Issue

The build was failing with these linker errors:

```
Error LNK2001: unresolved external symbol "public: __cdecl ResourceMonitor::ResourceMonitor(...)"
Error LNK2001: unresolved external symbol "public: void * __cdecl ResourceMonitor::resourceConsumed(int,int)"
Error LNK2001: unresolved external symbol "public: void * __cdecl ResourceMonitor::resourceReturned(int,int)"
```

## Root Cause

The ResourceMonitor class was not being properly exported from the Infrastructure DLL. Other classes like Bitmap were using the `INFRA_API` macro to mark them for export, but ResourceMonitor was missing this crucial declaration.

## Fix Applied

1. Added the `INFRA_API` macro definition to ResourceMonitor.h (copied from Bitmap.h)
2. Added the `INFRA_API` declaration to the ResourceMonitor class:

```cpp
class INFRA_API ResourceMonitor {
```

## How It Works

- When the Infrastructure project is compiled with the `INFRASTRUCTURE_EXPORTS` preprocessor definition, the `INFRA_API` macro expands to `__declspec(dllexport)`, which tells the compiler to export the class symbols.
- When other projects (like AgentLib) include the ResourceMonitor.h header, the `INFRA_API` macro expands to `__declspec(dllimport)`, which tells the compiler to import these symbols from a DLL.

## Verification

After this fix, the AgentLib project should successfully link against the ResourceMonitor implementation in LogZillaInfra.dll.

## Remaining Setup Errors

The Setup project errors may still occur if the build outputs are not in the expected locations. Make sure the files are copied to the correct locations specified in the Product.wxs file, or update the Product.wxs file to point to the correct locations.

## Note for Future Development

Always remember to use the `INFRA_API` macro when defining new public classes in the Infrastructure project that need to be used by other projects in the solution. This ensures proper exporting/importing of symbols across DLL boundaries.
