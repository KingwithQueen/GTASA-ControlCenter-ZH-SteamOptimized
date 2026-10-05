"""Build the portable release copy only; never start or discover GTA/trainer."""
from pathlib import Path
import argparse,ctypes as C,os,sys,json,shutil,socket,subprocess,hashlib,locale
if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf8',errors='replace')
if hasattr(sys.stderr,'reconfigure'):sys.stderr.reconfigure(encoding='utf8',errors='replace')
from ctypes import wintypes as W
from export_dictionary import export
from embed_manifest import embed
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def compiler(exe,project,output,port,node):
    with socket.socket() as sock:
        try:sock.bind(('127.0.0.1',port))
        except OSError:raise RuntimeError('Dedicated compiler port busy; no existing IDE will be used')
    os.environ['WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS']=f'--remote-debugging-port={port} --remote-debugging-address=127.0.0.1'
    os.environ['WEBVIEW2_USER_DATA_FOLDER']=str(output/'webview')
    os.environ['TB_ADDIN_TEST']='1'
    k=C.WinDLL('kernel32',use_last_error=True);u=C.WinDLL('user32',use_last_error=True)
    class SI(C.Structure):
        _fields_=[('cb',W.DWORD),('reserved',W.LPWSTR),('desktop',W.LPWSTR),('title',W.LPWSTR),('x',W.DWORD),('y',W.DWORD),('xs',W.DWORD),('ys',W.DWORD),('xc',W.DWORD),('yc',W.DWORD),('fill',W.DWORD),('flags',W.DWORD),('show',W.WORD),('cb2',W.WORD),('reserved2',C.POINTER(C.c_byte)),('stdin',W.HANDLE),('stdout',W.HANDLE),('stderr',W.HANDLE)]
    class PI(C.Structure):_fields_=[('process',W.HANDLE),('thread',W.HANDLE),('pid',W.DWORD),('tid',W.DWORD)]
    class BASIC(C.Structure):
        _fields_=[('processTime',C.c_int64),('jobTime',C.c_int64),('flags',W.DWORD),('minWorkingSet',C.c_size_t),('maxWorkingSet',C.c_size_t),('activeLimit',W.DWORD),('affinity',C.c_size_t),('priority',W.DWORD),('scheduling',W.DWORD)]
    class IO(C.Structure):_fields_=[(n,C.c_uint64) for n in ['readOps','writeOps','otherOps','readBytes','writeBytes','otherBytes']]
    class LIMIT(C.Structure):_fields_=[('basic',BASIC),('io',IO),('processMemory',C.c_size_t),('jobMemory',C.c_size_t),('peakProcessMemory',C.c_size_t),('peakJobMemory',C.c_size_t)]
    u.CreateDesktopW.argtypes=[W.LPCWSTR,W.LPCWSTR,C.c_void_p,W.DWORD,W.DWORD,C.c_void_p];u.CreateDesktopW.restype=W.HANDLE
    u.CloseDesktop.argtypes=[W.HANDLE]
    k.CreateJobObjectW.argtypes=[C.c_void_p,W.LPCWSTR];k.CreateJobObjectW.restype=W.HANDLE
    k.SetInformationJobObject.argtypes=[W.HANDLE,C.c_int,C.c_void_p,W.DWORD]
    k.CreateProcessW.argtypes=[W.LPCWSTR,W.LPWSTR,C.c_void_p,C.c_void_p,W.BOOL,W.DWORD,C.c_void_p,W.LPCWSTR,C.POINTER(SI),C.POINTER(PI)]
    k.AssignProcessToJobObject.argtypes=[W.HANDLE,W.HANDLE]
    k.ResumeThread.argtypes=[W.HANDLE];k.CloseHandle.argtypes=[W.HANDLE];k.TerminateProcess.argtypes=[W.HANDLE,W.UINT]
    k.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD];k.WaitForSingleObject.restype=W.DWORD
    desktop_name='LegacySourceRestore-'+str(os.getpid())
    desktop=u.CreateDesktopW(desktop_name,None,None,0,0x01FF,None)
    if not desktop:raise C.WinError(C.get_last_error())
    job=k.CreateJobObjectW(None,None);si=SI();si.cb=C.sizeof(si);si.desktop=desktop_name;pi=PI()
    try:
        limit=LIMIT();limit.basic.flags=0x2000
        if not job or not k.SetInformationJobObject(job,9,C.byref(limit),C.sizeof(limit)):raise C.WinError(C.get_last_error())
        command=C.create_unicode_buffer(subprocess.list2cmdline([str(exe),str(project/'GTASAConsole.vbp')]))
        if not k.CreateProcessW(str(exe),command,None,None,False,0x4,None,str(project),C.byref(si),C.byref(pi)):raise C.WinError(C.get_last_error())
        if not k.AssignProcessToJobObject(job,pi.process):
            k.TerminateProcess(pi.process,2);raise C.WinError(C.get_last_error())
        k.ResumeThread(pi.thread)
        print(json.dumps({'phase':'compiler','private_desktop':True,'compiler_pid':pi.pid,'trainer_launched':False}),flush=True)
        result=subprocess.run([node,str(ROOT/'scripts/build_ide.mjs'),str(project),str(port),str(output/'compiler-report.json')],cwd=ROOT,timeout=300)
        if result.returncode:raise RuntimeError('Compiler driver failed; see compiler-report.json')
    finally:
        if job:k.CloseHandle(job)
        if pi.thread:k.CloseHandle(pi.thread)
        if pi.process:
            # Closing the job requests termination; wait until the compiler has
            # released the linked EXE before updating its resources.
            k.WaitForSingleObject(pi.process,10000)
            k.CloseHandle(pi.process)
        u.CloseDesktop(desktop)
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--twinbasic',type=Path,default=os.environ.get('TWINBASIC_EXE'))
    p.add_argument('--output',type=Path,default=ROOT/'.work/restore-output')
    p.add_argument('--node',default='node');p.add_argument('--port',type=int,default=19551)
    p.add_argument('--prepare-only',action='store_true')
    a=p.parse_args();output=a.output.resolve()
    if output==ROOT or ROOT/'GTASAConsole' in output.parents or output==ROOT/'GTASAConsole':raise ValueError('Output must not overwrite source')
    if output.exists():raise ValueError('Output already exists; choose a new output directory')
    if not a.prepare_only and (not a.twinbasic or not a.twinbasic.resolve().is_file()):raise ValueError('Set TWINBASIC_EXE or --twinbasic to downloaded beta 991 compiler')
    output.mkdir(parents=True)
    temp=output/'temp';temp.mkdir()
    os.environ.update({'TEMP':str(temp),'TMP':str(temp),'PYTHONDONTWRITEBYTECODE':'1','LEGACY_SOURCE_ROOT':str(ROOT),'LEGACY_BUILD_OUTPUT':str(output)})
    lock=json.loads((ROOT/'tools.lock.json').read_text(encoding='utf8'))
    project=output/'GTASAConsole';shutil.copytree(ROOT/'GTASAConsole',project)
    for name,expected in lock['pinned_runtime_components'].items():
        source=ROOT/'vendor/vb6-controls'/name
        if sha(source)!=expected['sha256']:raise ValueError('Pinned OCX hash mismatch: '+name)
        shutil.copy2(source,project/name)
    # OLE Automation is supplied by Windows. The source VBP uses a relative file.
    windows=Path(os.environ['SystemRoot']);tlb=windows/'SysWOW64/stdole2.tlb'
    if not tlb.is_file():tlb=windows/'System32/stdole2.tlb'
    if not tlb.is_file():raise FileNotFoundError('Windows OLE Automation typelib missing')
    shutil.copy2(tlb,project/'stdole2.tlb')
    # Resolve the portable OLE reference inside the generated copy only. The
    # beta 991 importer otherwise emits a relative-path conversion warning.
    vbp=project/'GTASAConsole.vbp'
    portable=vbp.read_bytes()
    marker=b'#stdole2.tlb#'
    if portable.count(marker)!=1:raise ValueError('Unexpected portable OLE reference')
    vbp.write_bytes(portable.replace(marker,b'#'+str(project/'stdole2.tlb').encode('mbcs')+b'#'))
    export(project/'zh-CN.tsv')
    shutil.copy2(ROOT/'resources/defaults/GTASAConsole.default.ini',project/'GTASAConsole.ini')
    shutil.copy2(ROOT/'resources/defaults/GTASAConfig.default.dat',project/'GTASAConfig.dat')
    if a.prepare_only:
        print(json.dumps({'prepared':True,'project':str(project),'trainer_launched':False}));return
    print(json.dumps({'phase':'native','source_from_backup_only':True}),flush=True)
    native=subprocess.run([os.environ['COMSPEC'],'/d','/c',str(ROOT/'scripts/build-native.cmd')],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding=locale.getpreferredencoding(False),errors='replace',timeout=180)
    (output/'native-build.log').write_text(native.stdout,encoding='utf8')
    print(native.stdout[-3500:],flush=True)
    if native.returncode:raise RuntimeError('Native build/test failed; see native-build.log')
    shutil.copy2(output/'native/GTASASteam.dll',project/'GTASASteam.dll')
    compiler(a.twinbasic.resolve(),project,output,a.port,a.node)
    exe=project/'GTASA_Center_ZH.exe'
    metadata=embed(exe,ROOT/'GTASAConsole/app-local-controls.manifest')
    report=json.loads((output/'compiler-report.json').read_text(encoding='utf8'))
    assert report['success'] and 'DIAGNOSTICS\n0\n5\n0\n0' in report['buildState'],'Unexpected compiler diagnostics; review compiler report'
    result={'success':True,'source_from_backup_only':True,'native_unit_test_passed':True,'compiler':'twinBASIC beta 991 Win32','compile_errors':0,'preexisting_warnings':5,'trainer_launched':False,'real_gta_accessed':False,'exe_bytes':exe.stat().st_size,'exe_sha256':sha(exe),'native_dll_sha256':sha(project/'GTASASteam.dll'),'binary_identical_to_original_required':False,**metadata}
    (output/'build-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(json.dumps(result),flush=True)
if __name__=='__main__':main()
