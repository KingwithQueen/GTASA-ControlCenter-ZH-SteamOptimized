"""Capture only handles supplied by the owned offline fixture; no window discovery."""
import ctypes as C
from ctypes import wintypes as W
import struct, time, zlib

def capture_pages(project, output, process):
    u=C.WinDLL('user32',use_last_error=True);g=C.WinDLL('gdi32',use_last_error=True)
    for name,rest,args in (
        ('GetWindowThreadProcessId',W.DWORD,[W.HWND,C.POINTER(W.DWORD)]),
        ('IsWindowVisible',W.BOOL,[W.HWND]),
        ('GetWindowRect',W.BOOL,[W.HWND,C.POINTER(W.RECT)]),
        ('GetWindowDC',C.c_void_p,[W.HWND]),('ReleaseDC',C.c_int,[W.HWND,C.c_void_p]),
        ('PrintWindow',W.BOOL,[W.HWND,C.c_void_p,W.UINT])):
        f=getattr(u,name);f.restype=rest;f.argtypes=args
    for name,rest,args in (
        ('CreateCompatibleDC',C.c_void_p,[C.c_void_p]),
        ('CreateCompatibleBitmap',C.c_void_p,[C.c_void_p,C.c_int,C.c_int]),
        ('SelectObject',C.c_void_p,[C.c_void_p,C.c_void_p]),
        ('GetDIBits',C.c_int,[C.c_void_p,C.c_void_p,W.UINT,W.UINT,C.c_void_p,C.c_void_p,W.UINT]),
        ('DeleteObject',W.BOOL,[C.c_void_p]),('DeleteDC',W.BOOL,[C.c_void_p])):
        f=getattr(g,name);f.restype=rest;f.argtypes=args
    def owned(handle):
        pid=W.DWORD();u.GetWindowThreadProcessId(handle,C.byref(pid))
        assert pid.value==process.dwProcessId, 'Fixture does not own capture handle'
    def rect(handle):
        owned(handle);r=W.RECT();assert u.GetWindowRect(handle,C.byref(r));return r
    def png(handle,path):
        r=rect(handle);width,height=r.right-r.left,r.bottom-r.top
        assert 300<width<4000 and 300<height<4000
        dc=u.GetWindowDC(handle);mem=g.CreateCompatibleDC(dc)
        bitmap=g.CreateCompatibleBitmap(dc,width,height);old=g.SelectObject(mem,bitmap)
        try:
            assert u.PrintWindow(handle,mem,2)
            header=(C.c_long*10)();header[0]=40;header[1]=width;header[2]=-height;header[3]=1|(32<<16)
            pixels=C.create_string_buffer(width*height*4)
            assert g.GetDIBits(mem,bitmap,0,height,pixels,header,0)==height
            raw=pixels.raw;rows=[]
            for y in range(height):
                row=raw[y*width*4:(y+1)*width*4]
                rgb=bytearray(width*3)
                rgb[0::3]=row[2::4];rgb[1::3]=row[1::4];rgb[2::3]=row[0::4]
                rows.append(b'\0'+rgb)
            def chunk(name,data):
                return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
            path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+
                             chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))
        finally:
            g.SelectObject(mem,old);g.DeleteObject(bitmap);g.DeleteDC(mem);u.ReleaseDC(handle,dc)
    for page in range(8):
        marker=project/'completion-page.ready'
        for _ in range(1000):
            receipt=(project/'offline-feature-ui.txt').read_text(encoding='cp1252') if (project/'offline-feature-ui.txt').exists() else ''
            if 'FAIL ' in receipt:raise RuntimeError(receipt)
            lines=marker.read_text(encoding='ascii').splitlines() if marker.exists() else []
            if len(lines)==33 and lines[0].startswith(str(page)+'|'):break
            time.sleep(.05)
        else:raise RuntimeError('Page capture timed out: '+str(page))
        _,window,tab,actual_page=map(int,lines[0].split('|'));window&=0xffffffff;tab&=0xffffffff
        png(window,output/('page-'+str(page)+'.png'))
        state=[]
        for line in lines[1:]:
            index,toggle,lock=map(int,line.split('|'));toggle&=0xffffffff;lock&=0xffffffff
            owned(toggle);owned(lock)
            state.append((index,toggle,lock,bool(u.IsWindowVisible(toggle)),bool(u.IsWindowVisible(lock))))
        (output/('page-'+str(page)+'-visibility.txt')).write_text(str(actual_page)+'\n'+repr(state),encoding='ascii')
        assert all(t== (actual_page==1) and l==(actual_page==1) for _,_,_,t,l in state), 'Native page visibility differs; see visibility receipt'
        if page in (1,6,7):
            bounds=rect(tab)
            for line in lines[1:]:
                _,toggle,lock=map(int,line.split('|'))
                for handle in (toggle,lock):
                    r=rect(handle&0xffffffff)
                    assert bounds.left<=r.left<r.right<=bounds.right and bounds.top<=r.top<r.bottom<=bounds.bottom, 'Control outside player page'
        time.sleep(.1);png(window,output/('page-'+str(page)+'.png'))
        (project/'completion-page.ack').write_text(str(page),encoding='ascii')
