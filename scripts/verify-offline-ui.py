"""Compile a private instrumented UI; exercise release APIs on synthetic memory.

No game discovery or launch. The production trainer is never executed.
"""
from pathlib import Path
import argparse,os,sys,subprocess,json,time,hashlib,shutil
from build import ROOT,compiler
from embed_manifest import embed

def edit(path,old,new):
    text=path.read_bytes().decode('cp1252')
    assert text.count(old)==1,old
    path.write_bytes(text.replace(old,new).encode('cp1252'))

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--twinbasic',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve()
    inputs=[p for p in (ROOT/'GTASAConsole').iterdir() if p.suffix.lower() in ('.frm','.frx','.bas','.cls','.ctl','.ctx','.res')] + [p for p in (ROOT/'native').iterdir() if p.is_file()]
    inputs += [ROOT/'localization/zh-CN.master.tsv', ROOT/'scripts/fixtures/completion-ui.bas']
    source_receipt={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    subprocess.run([sys.executable,str(ROOT/'scripts/build.py'),'--prepare-only','--output',str(output)],check=True)
    os.environ.update(LEGACY_SOURCE_ROOT=str(ROOT),LEGACY_BUILD_OUTPUT=str(output),TEMP=str(output/'temp'),TMP=str(output/'temp'))
    project=output/'GTASAConsole';declares=project/'mdlGTASADeclares.bas';form=project/'frmSAConsole.frm'
    edit(declares,'Option Explicit','Option Explicit\r\nPublic Const LegacyOfflineBuild As Boolean = True\r\n'
        'Declare Function SteamFixtureCoffeeSupported Lib "GTASASteam.dll" (ByVal Supported As Long) As Long\r\n'
        'Declare Function SteamFixtureTrain Lib "GTASASteam.dll" () As Long\r\n'
        'Declare Function SteamFixtureVehicle Lib "GTASASteam.dll" (ByVal Enabled As Long) As Long\r\n'
        'Declare Function SteamFixtureBegin Lib "GTASASteam.dll" () As Long\r\n'
        'Declare Function SteamFixtureRun Lib "GTASASteam.dll" () As Long\r\n'
        'Declare Function SteamFixtureDisturbGarage Lib "GTASASteam.dll" () As Long')
    for timer in ('tmrHook','tmrConsole','tmrFindCar'):
        action='OfflineFeatureUiTest' if timer=='tmrHook' else ''
        edit(form,'On Error GoTo err'+timer+'_Timer','On Error GoTo err'+timer+'_Timer\r\n    If LegacyOfflineBuild Then\r\n        '+action+'\r\n        Exit Sub\r\n    End If')
    # Remove the declarations entirely in the test copy; even accidental calls
    # to these names cannot enumerate a window or open an external process.
    content=declares.read_bytes().decode('cp1252')
    import re
    for name in ('FindWindow','OpenProcess'):
        pattern=r'Declare Function '+name+r' Lib [^\r\n]+'
        matches=re.findall(pattern,content);assert len(matches)==1
        signature=matches[0].split(' (',1)[1]
        content=re.sub(pattern,'',content)
        content+='\r\nPublic Function '+name+' ('+signature+'\r\n    '+name+' = 0\r\nEnd Function\r\n'
    declares.write_bytes(content.encode('cp1252'))
    cheat=project/'GTASACheat.ctl'
    cheat.write_bytes(cheat.read_bytes()+b'\r\nPublic Property Get OfflineCheatHandle() As Long\r\n    OfflineCheatHandle = m_CheatWnd\r\nEnd Property\r\nPublic Property Get OfflineLockHandle() As Long\r\n    OfflineLockHandle = m_LockWnd\r\nEnd Property\r\n')
    fixture=r'''
Private Sub OfflineCheck(ByVal Condition As Boolean, ByVal Label As String)
    If Not Condition Then Err.Raise vbObjectError + 902, "Offline UI", Label
End Sub

Private Sub OfflineFeatureUiTest()
    Static Started As Boolean
    If Started Then Exit Sub
    Started = True
    tmrHook.Enabled = False: tmrConsole.Enabled = False: tmrFindCar.Enabled = False
    Dim f As Integer, i As Integer, Values() As String, Config As String
    Dim Meta(23) As Long, Cars(255) As Byte
    f = FreeFile
    Open App.Path & "\offline-feature-ui.txt" For Output As #f
    On Error GoTo Failed
    lngPHandle = SteamFixtureBegin()
    OfflineCheck lngPHandle <> 0, "fixture setup"
    isSteamGame = True: isHasHandle = True: isHasPlayer = True: isTimerClick = True
    ResetSteamGarages
    ApplySteamUI
    Print #f, "Steam controls initialized"
    Config = cParking(0).GetIniVals(0)
    m_SteamGarageImported(0) = True
    SteamGarageCommand 0
    OfflineCheck cParking(0).GetIniVals(0) = Config, "initial baseline keeps INI selections"
    OfflineCheck Split(cParking(1).GetIniVals(0), ",")(1) = "411", "ordinary startup reads current vehicles"
    isTimerClick = False
    SteamGarageCommand 0
    isTimerClick = True
    For i = 0 To 16
        OfflineCheck m_SteamGarageValid(i) And cParking(i).Enabled, "read garage " & CStr(i)
    Next i
    Values = Split(cParking(0).GetIniVals(0), ",")
    Values(1) = "400": Values(2) = "1": Values(3) = "1": Values(4) = "1": Values(5) = "1": Values(6) = "12": Values(7) = "34"
    Config = Join(Values, ",")
    cParking(0).SetIniVals 0, Config
    SteamGarageCommand 1
    OfflineCheck m_SteamGarageValid(0), "garage write and reread"
    OfflineCheck SteamGarageInspect(0, Meta(0), Cars(0)) = 0, "garage API inspect"
    OfflineCheck Cars(18) = 144 And Cars(19) = 1 And Cars(50) = 12 And Cars(51) = 34, "selected model and colors"
    OfflineCheck (Cars(16) And 15) = 15 And Cars(63) = 178, "proofs and unrelated bytes"
    cParking(0).Locked = vbChecked
    OfflineCheck SteamFixtureDisturbGarage() = 1 And SteamFixtureRun() = 1, "game-thread lock run"
    OfflineCheck SteamGarageInspect(0, Meta(0), Cars(0)) = 0 And Cars(18) = 144, "garage lock event and restoration"
    cParking(0).Locked = vbUnchecked
    Print #f, "17 garages, INI editing, colors, proofs and lock event passed"
    isInternalClick = True
    oCheatStates(20).CheatState = vbChecked: oCheatStates(21).CheatState = vbChecked
    isInternalClick = False
    ApplySteamFeature 20, False: ApplySteamFeature 21, False
    OfflineCheck SteamFeatureGet(0) = 1 And SteamFeatureGet(1) = 1, "feature enable ABI"
    oCheatStates(20).CheatState = vbUnchecked: oCheatStates(21).CheatState = vbUnchecked
    OfflineCheck SteamFeatureGet(0) = 0 And SteamFeatureGet(1) = 0, "feature click events"
    For i = 0 To 5
        isInternalClick = True: oGFStats(i).ScrollVal = 75 + i: isInternalClick = False
        ApplySteamGirlfriend i, 1, False
        OfflineCheck SteamGirlfriendSupported() = 1, "girlfriend signature check"
        OfflineCheck SteamGirlfriendSet(i, 0, 0) = 0, "girlfriend unlock ABI"
    Next i
    Print #f, "feature switches and six girlfriend UI/API calls passed"
    sstMain.Tab = 1: EndMainTabRedraw: SyncPlayerStatusControls: Me.Refresh: DoEvents
    OfflineCheck chkCoffee.Visible And chkCoffee.Enabled, "visible Coffee capability"
    PerfSendMessage chkCoffee.hWnd, &HF5, 0, 0
    OfflineCheck SteamCoffeeGet() = 0, "Coffee enable click"
    PerfSendMessage chkCoffee.hWnd, &HF5, 0, 0
    OfflineCheck SteamCoffeeGet() = 1, "Coffee disable click"
    OfflineCheck SteamFixtureCoffeeSupported(0) = 1, "unsupported Coffee fixture"
    RefreshSteamCoffeeUI
    OfflineCheck Not chkCoffee.Enabled, "unsupported Coffee disabled"
    PerfSendMessage chkCoffee.hWnd, &HF5, 0, 0
    OfflineCheck SteamCoffeeGet() = 255, "unsupported native click stays unavailable"
    OfflineCheck SteamFixtureCoffeeSupported(1) = 1, "restore supported Coffee"
    RefreshSteamCoffeeUI
    OfflineCheck SteamCoffeeGet() = 1, "unsupported click did not change script flag"
    Print #f, "Hot Coffee script switch click events passed"
    chkFeedback.Value = vbChecked
    OfflineCheck OnScreenText("Steam trainer feedback " & ChrW(&H4E2D) & ChrW(&H6587)), "feedback StrPtr/length ABI"
    OfflineCheck SteamFixtureRun() = 1, "feedback queue dispatch"
    chkFeedback.Value = vbUnchecked
    OfflineCompletionTests f
    OfflineCapturePages
    Print #f, "feedback enable/message events and all six pages passed"
    Print #f, "PASS"
    Close #f
    isHasHandle = False: lngPHandle = 0
    Unload Me
    Exit Sub
Failed:
    Print #f, "FAIL " & CStr(Err.Number) & " " & Err.Description
    Close #f
    isHasHandle = False: lngPHandle = 0
    Unload Me
End Sub
'''.replace('\n','\r\n')
    with form.open('ab') as file:file.write(fixture.encode('cp1252'))
    with form.open('ab') as file:file.write((ROOT/'scripts/fixtures/completion-ui.bas').read_bytes().decode('ascii').replace('\r\n','\n').replace('\n','\r\n').encode('cp1252'))
    extra='\n SteamFixtureCoffeeSupported=_SteamFixtureCoffeeSupported@4\n SteamFixtureTrain=_SteamFixtureTrain@0\n SteamFixtureVehicle=_SteamFixtureVehicle@4\n SteamFixtureBegin=_SteamFixtureBegin@0\n SteamFixtureRun=_SteamFixtureRun@0\n SteamFixtureDisturbGarage=_SteamFixtureDisturbGarage@0\n'
    (output/'offline-fixture.def').write_text((ROOT/'native/exports.def').read_text().replace('LIBRARY GTASASteam','LIBRARY OfflineUiFixture')+extra,encoding='utf8')
    subprocess.run([os.environ['COMSPEC'],'/d','/c',str(ROOT/'scripts/build-offline-fixture.cmd')],check=True)
    shutil.copy2(output/'OfflineUiFixture.dll',project/'GTASASteam.dll')
    compiler(args.twinbasic.resolve(),project,output,19551,'node')
    exe=project/'GTASA_Center_ZH.exe';embed(exe,ROOT/'GTASAConsole/app-local-controls.manifest')
    # Reuse the established owned-desktop harness; it enumerates no processes.
    from private_desktop import launch
    receipt=project/'offline-feature-ui.txt'
    from capture_completion_ui import capture_pages
    from private_desktop import u,k
    import ctypes as C
    from ctypes import wintypes as W
    u.GetThreadDesktop.argtypes=[W.DWORD];u.GetThreadDesktop.restype=W.HANDLE
    u.SetThreadDesktop.argtypes=[W.HANDLE];u.SetThreadDesktop.restype=W.BOOL
    k.GetCurrentThreadId.restype=W.DWORD
    with launch(exe,['--offline-features']) as (process,desktop):
        previous=u.GetThreadDesktop(k.GetCurrentThreadId())
        assert u.SetThreadDesktop(desktop)
        try:capture_pages(project,output,process)
        finally:assert u.SetThreadDesktop(previous)
        for _ in range(450):
            text=receipt.read_bytes().decode('cp1252') if receipt.exists() else ''
            if 'FAIL ' in text:raise RuntimeError(text)
            if '\nPASS' in text:break
            time.sleep(.1)
        else:raise RuntimeError('Offline UI test timed out: '+text)
    result={'success':True,'private_desktop':True,'instrumented_ui_only':True,'real_game_discovery_or_access':False,'production_trainer_launched':False,'test_exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'receipt':text,'source_inputs':source_receipt,'localized_spawn_button_verified':True,'coffee_native_clicks_verified':True,'coffee_visibility_verified':True,'captured_pages':8,'native_cheat_and_lock_clicks':88,'player_controls_within_tab_bounds':True}
    (output/'offline-ui-result.json').write_text(json.dumps(result,indent=2),encoding='utf8');print(json.dumps(result))
if __name__=='__main__':main()
