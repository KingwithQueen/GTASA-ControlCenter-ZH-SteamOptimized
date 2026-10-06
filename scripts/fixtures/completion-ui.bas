Private Sub OfflineCompletionTests(ByVal ReceiptFile As Integer)
    Dim i As Integer, n As Integer, Car As Long, OldTexture As Long, Config As String
    sstMain.Tab = 1: EndMainTabRedraw: SyncPlayerStatusControls: Me.Refresh: DoEvents
    OfflineCheck ReFillPlayerAdr(), "synthetic player addresses"
    For i = 0 To 21
        OfflineCheck oCheatStates(i).Enabled, "enabled toggle " & CStr(i)
        OfflineCheck PerfIsWindowVisible(oCheatStates(i).OfflineCheatHandle) <> 0, "visible toggle " & CStr(i)
        OfflineCheck PerfIsWindowVisible(oCheatStates(i).OfflineLockHandle) <> 0, "visible lock " & CStr(i)
        isInternalClick = True
        oCheatStates(i).CheatState = vbUnchecked: oCheatStates(i).CheatLock = vbUnchecked
        isInternalClick = False
        PerfSendMessage oCheatStates(i).OfflineCheatHandle, &HF5, 0, 0
        OfflineCheck oCheatStates(i).CheatState = vbChecked, "native toggle click " & CStr(i)
        If i < 20 Then
            OfflineCheck GetMemByte(GTASABaseAdr.cCheatsAdr(i)) = 1, "cheat flag on " & CStr(i)
        Else
            OfflineCheck SteamFeatureGet(i - 20) = 1, "native feature on " & CStr(i)
        End If
        PerfSendMessage oCheatStates(i).OfflineLockHandle, &HF5, 0, 0
        OfflineCheck oCheatStates(i).CheatLock = vbChecked, "native lock click " & CStr(i)
        Config = String$(16, Chr$(0))
        n = GetPrivateProfileString("Main", oCheatStates(i).Tag, "", Config, 16, strIniFileName)
        OfflineCheck Left$(Config, n) = "1,1", "lock INI event " & CStr(i)
        PerfSendMessage oCheatStates(i).OfflineLockHandle, &HF5, 0, 0
        PerfSendMessage oCheatStates(i).OfflineCheatHandle, &HF5, 0, 0
        OfflineCheck oCheatStates(i).CheatState = vbUnchecked And oCheatStates(i).CheatLock = vbUnchecked, "native clicks off " & CStr(i)
        If i < 20 Then OfflineCheck GetMemByte(GTASABaseAdr.cCheatsAdr(i)) = 0, "cheat flag off " & CStr(i)
    Next i
    For i = 22 To 31: OfflineCheck Not oCheatStates(i).Enabled, "reserved control disabled": Next i
    Print #ReceiptFile, "22 native toggle and lock clicks, flags, persisted locks and 10 reserved controls passed"
    isFixWeaponSlots(2) = True: iFixWeaponID(2) = 24: iFixWeaponAmmo(2) = 100
    SetMemLong GTASAPlayerAddresse.lngWeaponsAdr(2), 24
    SetMemLong GTASAPlayerAddresse.lngWeaponsAdr(2) + 4, 77
    SetMemLong GTASAPlayerAddresse.lngWeaponsAdr(2) + 8, 7
    SetMemLong GTASAPlayerAddresse.lngWeaponsAdr(2) + 12, 99
    MaintainWeaponLocks
    OfflineCheck GetMemLong(GTASAPlayerAddresse.lngWeaponsAdr(2) + 12) = 100, "same weapon consumed ammo restored"
    SetMemLong GTASAPlayerAddresse.lngWeaponsAdr(2) + 12, 0
    MaintainWeaponLocks
    OfflineCheck GetMemLong(GTASAPlayerAddresse.lngWeaponsAdr(2) + 12) = 100, "empty ammo restored"
    OfflineCheck GetMemLong(GTASAPlayerAddresse.lngWeaponsAdr(2) + 4) = 77 And GetMemLong(GTASAPlayerAddresse.lngWeaponsAdr(2) + 8) = 7, "ammo unrelated fields preserved"
    isFixWeaponSlots(2) = False
    n = cParking(0).CarCount: Config = cParking(0).GetIniVals(0)
    OfflineCheck GarageListMatrix(0, 537) = 0 And SteamGarageModelSupported(537) = 0, "train unavailable without component"
    OfflineCheck SteamFixtureTrain() = 1, "seed train component"
    ApplySteamUI
    OfflineCheck cParking(0).CarCount = n + 6 And GarageListMatrix(0, 537) >= n, "six train models appended"
    OfflineCheck cParking(0).GetIniVals(0) = Config, "append preserves current garage selection"
    n = cParking(0).CarCount: ApplySteamUI
    OfflineCheck cParking(0).CarCount = n, "no duplicate special models"
    Print #ReceiptFile, "ammo restoration, preserved weapon fields and conditional train catalog passed"
    Car = SteamFixtureVehicle(1): OfflineCheck Car <> 0, "synthetic car and trailer"
    SetCarAdrEx tCurrCarAdr, Car
    SetCarAdrEx tCurrTrailer, GetMemLong(Car + 1224)
    isHasCar = True: isHadCar = True
    OldTexture = GetMemLong(Car + 1416)
    txtLicensePlate.Text = "CJ 123"
    cmdSetPlate_Click
    OfflineCheck m_SteamPlatePending And SteamPlateStatus() = 8, "plate button queued"
    OfflineCheck GetMemLong(Car + 1416) = OldTexture, "old texture kept until dispatch"
    OfflineCheck SteamFixtureRun() = 1, "plate game thread"
    RefreshSteamPlateUI
    OfflineCheck Not m_SteamPlatePending And GetMemLong(Car + 1416) <> OldTexture, "plate UI acknowledges replacement"
    For i = 0 To 2
        SetMemFloat tCurrCarAdr.lngCarSpeedAdr + i * 4, 3
        SetMemFloat tCurrCarAdr.lngCarSpinAdr + i * 4, 4
        SetMemFloat tCurrTrailer.lngCarSpeedAdr + i * 4, 5
        SetMemFloat tCurrTrailer.lngCarSpinAdr + i * 4, 6
    Next i
    OfflineCheck PasteWarpLoc(0, "100;100;20;0"), "real vehicle and trailer teleport path"
    For i = 0 To 2
        OfflineCheck GetMemFloat(tCurrCarAdr.lngCarSpeedAdr + i * 4) = 0 And GetMemFloat(tCurrCarAdr.lngCarSpinAdr + i * 4) = 0, "car motion cleared"
        OfflineCheck GetMemFloat(tCurrTrailer.lngCarSpeedAdr + i * 4) = 0 And GetMemFloat(tCurrTrailer.lngCarSpinAdr + i * 4) = 0, "trailer motion cleared"
    Next i
    OfflineCheck SteamFixtureVehicle(0) = 1, "restore player on foot"
    Print #ReceiptFile, "plate button/game-thread/UI acknowledgement and car/trailer teleport velocities passed"
End Sub

Private Sub OfflineCapturePages()
    Dim i As Integer, j As Integer, Page As Integer, f As Integer, Ack As String, Start As Single
    For i = 0 To 7
        Page = i
        If i = 6 Or i = 7 Then Page = 1
        If i = 6 Then Me.Width = Me.Width + 600: Me.Height = Me.Height + 300
        If i = 7 Then Me.Width = Me.Width - 600: Me.Height = Me.Height - 300
        sstMain.Tab = Page: EndMainTabRedraw: SyncPlayerStatusControls: Form_Resize: Me.Refresh: DoEvents: SyncPlayerStatusControls
        OfflineCheck sstMain.Tab = Page, "requested page selected"
        f = FreeFile
        Open App.Path & "\completion-page.ready" For Output As #f
        Print #f, CStr(i) & "|" & CStr(Me.hWnd) & "|" & CStr(sstMain.hWnd) & "|" & CStr(sstMain.Tab)
        For j = 0 To 31
            Print #f, CStr(j) & "|" & CStr(oCheatStates(j).OfflineCheatHandle) & "|" & CStr(oCheatStates(j).OfflineLockHandle)
        Next j
        Close #f
        Start = Timer
        Do
            Ack = ""
            If Dir$(App.Path & "\completion-page.ack") <> "" Then
                f = FreeFile: Open App.Path & "\completion-page.ack" For Input As #f
                If Not EOF(f) Then Line Input #f, Ack
                Close #f
            End If
            DoEvents
            If Timer - Start > 25 Then Err.Raise vbObjectError + 904, , "page capture timed out"
        Loop Until Ack = CStr(i)
    Next i
End Sub
