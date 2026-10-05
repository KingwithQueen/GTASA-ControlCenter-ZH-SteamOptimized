"""Owned private-desktop process harness. Does not enumerate other desktops or processes."""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import os,subprocess
from contextlib import contextmanager
k=C.WinDLL('kernel32',use_last_error=True);u=C.WinDLL('user32',use_last_error=True)
class STARTUPINFO(C.Structure):
    _fields_=[('cb',W.DWORD),('lpReserved',W.LPWSTR),('lpDesktop',W.LPWSTR),('lpTitle',W.LPWSTR),('dwX',W.DWORD),('dwY',W.DWORD),('dwXSize',W.DWORD),('dwYSize',W.DWORD),('dwXCountChars',W.DWORD),('dwYCountChars',W.DWORD),('dwFillAttribute',W.DWORD),('dwFlags',W.DWORD),('wShowWindow',W.WORD),('cbReserved2',W.WORD),('lpReserved2',C.POINTER(W.BYTE)),('hStdInput',W.HANDLE),('hStdOutput',W.HANDLE),('hStdError',W.HANDLE)]
class PROCESSINFO(C.Structure):
    _fields_=[('hProcess',W.HANDLE),('hThread',W.HANDLE),('dwProcessId',W.DWORD),('dwThreadId',W.DWORD)]
class BASIC_LIMIT(C.Structure):
    _fields_=[('PerProcessUserTimeLimit',C.c_int64),('PerJobUserTimeLimit',C.c_int64),('LimitFlags',W.DWORD),('MinimumWorkingSetSize',C.c_size_t),('MaximumWorkingSetSize',C.c_size_t),('ActiveProcessLimit',W.DWORD),('Affinity',C.c_size_t),('PriorityClass',W.DWORD),('SchedulingClass',W.DWORD)]
class IOC(C.Structure):
    _fields_=[(n,C.c_uint64) for n in ['ReadOperationCount','WriteOperationCount','OtherOperationCount','ReadTransferCount','WriteTransferCount','OtherTransferCount']]
class LIMIT(C.Structure):
    _fields_=[('BasicLimitInformation',BASIC_LIMIT),('IoInfo',IOC),('ProcessMemoryLimit',C.c_size_t),('JobMemoryLimit',C.c_size_t),('PeakProcessMemoryUsed',C.c_size_t),('PeakJobMemoryUsed',C.c_size_t)]
u.CreateDesktopW.argtypes=[W.LPCWSTR,W.LPCWSTR,C.c_void_p,W.DWORD,W.DWORD,C.c_void_p];u.CreateDesktopW.restype=W.HANDLE
u.CloseDesktop.argtypes=[W.HANDLE]
k.CreateJobObjectW.argtypes=[C.c_void_p,W.LPCWSTR];k.CreateJobObjectW.restype=W.HANDLE
k.SetInformationJobObject.argtypes=[W.HANDLE,C.c_int,C.c_void_p,W.DWORD]
k.CreateProcessW.argtypes=[W.LPCWSTR,W.LPWSTR,C.c_void_p,C.c_void_p,W.BOOL,W.DWORD,C.c_void_p,W.LPCWSTR,C.POINTER(STARTUPINFO),C.POINTER(PROCESSINFO)]
k.AssignProcessToJobObject.argtypes=[W.HANDLE,W.HANDLE]
k.ResumeThread.argtypes=[W.HANDLE];k.CloseHandle.argtypes=[W.HANDLE]
k.TerminateProcess.argtypes=[W.HANDLE,W.UINT]

@contextmanager
def launch(exe,args=()):
    desktop_name='LegacyPerfUI-'+str(os.getpid())
    desktop=u.CreateDesktopW(desktop_name,None,None,0,0x01FF,None);assert desktop,C.get_last_error()
    job=k.CreateJobObjectW(None,None);assert job,C.get_last_error()
    limit=LIMIT();limit.BasicLimitInformation.LimitFlags=0x2000
    assert k.SetInformationJobObject(job,9,C.byref(limit),C.sizeof(limit)),C.get_last_error()
    si=STARTUPINFO();si.cb=C.sizeof(si);si.lpDesktop=desktop_name;pi=PROCESSINFO()
    command=C.create_unicode_buffer(subprocess.list2cmdline([str(exe),*args]))
    try:
        assert k.CreateProcessW(str(exe),command,None,None,False,4,None,str(exe.parent),C.byref(si),C.byref(pi)),C.get_last_error()
        if not k.AssignProcessToJobObject(job,pi.hProcess):
            k.TerminateProcess(pi.hProcess,2);raise OSError(C.get_last_error(),'Cannot supervise offline test')
        k.ResumeThread(pi.hThread)
        yield pi,desktop
    finally:
        k.CloseHandle(job)
        if pi.hThread:k.CloseHandle(pi.hThread)
        if pi.hProcess:k.CloseHandle(pi.hProcess)
        u.CloseDesktop(desktop)
