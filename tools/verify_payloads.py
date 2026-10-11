import argparse
import ctypes
import hashlib
from pathlib import Path
from ctypes import wintypes


def verify(exe, payloads):
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    api.LoadLibraryExW.argtypes = [wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD]
    api.LoadLibraryExW.restype = wintypes.HMODULE
    api.FindResourceW.argtypes = [wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p]
    api.FindResourceW.restype = wintypes.HANDLE
    api.SizeofResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    api.SizeofResource.restype = wintypes.DWORD
    api.LoadResource.argtypes = [wintypes.HMODULE, wintypes.HANDLE]
    api.LoadResource.restype = wintypes.HANDLE
    api.LockResource.argtypes = [wintypes.HANDLE]
    api.LockResource.restype = ctypes.c_void_p
    api.FreeLibrary.argtypes = [wintypes.HMODULE]
    api.FreeLibrary.restype = wintypes.BOOL
    module = api.LoadLibraryExW(str(exe.resolve()), None, 2)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        for resource_id, path in payloads:
            resource = api.FindResourceW(module, resource_id, 10)
            if not resource:
                raise RuntimeError(f"Missing resource {resource_id}: {path.name}")
            size = api.SizeofResource(module, resource)
            pointer = api.LockResource(api.LoadResource(module, resource))
            if not pointer:
                raise ctypes.WinError(ctypes.get_last_error())
            embedded = ctypes.string_at(pointer, size)
            expected = path.read_bytes()
            if embedded != expected:
                raise RuntimeError(f"Embedded payload differs: {path}")
            print(f"{path.name}: {size} bytes, SHA256 {hashlib.sha256(embedded).hexdigest()}")
    finally:
        api.FreeLibrary(module)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compare launcher resources without executing the launcher.")
    parser.add_argument("launcher", type=Path)
    parser.add_argument("--dll", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--signatures", type=Path)
    args = parser.parse_args()
    payloads = [(201, args.dll), (203, args.core)]
    if args.signatures:
        payloads.append((206, args.signatures))
    verify(args.launcher, payloads)
