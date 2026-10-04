"""Embed the preserved app-local manifest with Windows resource APIs only."""
from pathlib import Path
import ctypes as C,struct
def embed(exe,manifest):
    exe=Path(exe);data=Path(manifest).read_bytes();raw=exe.read_bytes()
    pe=struct.unpack_from('<I',raw,0x3c)[0]
    assert raw[pe:pe+4]==b'PE\0\0'
    assert struct.unpack_from('<H',raw,pe+4)[0]==0x14c
    assert struct.unpack_from('<H',raw,pe+24)[0]==0x10b
    k=C.WinDLL('kernel32',use_last_error=True)
    k.BeginUpdateResourceW.argtypes=[C.c_wchar_p,C.c_int];k.BeginUpdateResourceW.restype=C.c_void_p
    k.UpdateResourceW.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_ushort,C.c_void_p,C.c_ulong];k.UpdateResourceW.restype=C.c_int
    k.EndUpdateResourceW.argtypes=[C.c_void_p,C.c_int];k.EndUpdateResourceW.restype=C.c_int
    h=k.BeginUpdateResourceW(str(exe),False)
    if not h:raise C.WinError(C.get_last_error())
    b=C.create_string_buffer(data)
    ok=k.UpdateResourceW(h,C.c_void_p(24),C.c_void_p(1),0,b,len(data))
    if not k.EndUpdateResourceW(h,not ok) or not ok:raise C.WinError(C.get_last_error())
    k.LoadLibraryExW.argtypes=[C.c_wchar_p,C.c_void_p,C.c_ulong];k.LoadLibraryExW.restype=C.c_void_p
    k.FindResourceW.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p];k.FindResourceW.restype=C.c_void_p
    k.SizeofResource.argtypes=[C.c_void_p,C.c_void_p];k.SizeofResource.restype=C.c_ulong
    k.LoadResource.argtypes=[C.c_void_p,C.c_void_p];k.LoadResource.restype=C.c_void_p
    k.LockResource.argtypes=[C.c_void_p];k.LockResource.restype=C.c_void_p
    k.FreeLibrary.argtypes=[C.c_void_p]
    module=k.LoadLibraryExW(str(exe),None,0x2) # LOAD_LIBRARY_AS_DATAFILE: never executes trainer code.
    if not module:raise C.WinError(C.get_last_error())
    try:
        r=k.FindResourceW(module,C.c_void_p(1),C.c_void_p(24))
        if not r:raise C.WinError(C.get_last_error())
        n=k.SizeofResource(module,r);ptr=k.LockResource(k.LoadResource(module,r))
        assert C.string_at(ptr,n)==data,'Embedded manifest differs'
    finally:k.FreeLibrary(module)
    return {'target':'x86 PE32','app_local_manifest_verified':True}
