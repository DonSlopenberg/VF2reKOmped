"""Verify packaged Windows icons without executing the binaries."""
import ctypes, struct, sys
from ctypes import wintypes
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
kernel.LoadLibraryExW.restype = wintypes.HMODULE
kernel.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
kernel.FindResourceW.restype = wintypes.HANDLE
kernel.LoadResource.argtypes = [wintypes.HMODULE,wintypes.HANDLE]
kernel.LoadResource.restype = wintypes.HANDLE
kernel.LockResource.argtypes = [wintypes.HANDLE]
kernel.LockResource.restype = ctypes.c_void_p
kernel.FreeLibrary.argtypes = [wintypes.HMODULE]
for path in sys.argv[1:]:
    module=kernel.LoadLibraryExW(path,None,0x22)
    assert module, (path,ctypes.get_last_error())
    try:
        resource=kernel.FindResourceW(module,1,14)
        assert resource, f'Missing app icon: {path}'
        ptr=kernel.LockResource(kernel.LoadResource(module,resource))
        _,kind,count=struct.unpack('<HHH',ctypes.string_at(ptr,6))
        assert kind==1 and count==7, (path,kind,count)
        entries=ctypes.string_at(ptr+6,count*14)
        sizes=[entries[i*14] or 256 for i in range(count)]
        assert sizes==[16,24,32,48,64,128,256], sizes
        print(f'PASS: {path}: icon sizes {sizes}')
    finally:
        kernel.FreeLibrary(module)
