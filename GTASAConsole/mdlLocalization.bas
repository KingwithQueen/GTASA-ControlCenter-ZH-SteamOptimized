Attribute VB_Name = "mdlLocalization"
Option Explicit

Private translations As Object
Private attempted As Boolean
Private fallbackCache As Object

Public Function ZH(ByVal original As String) As String
    On Error GoTo fallback
    Dim stream As Object, rows As Variant, row As Variant, fields As Variant
    Dim key As Variant, best As String
    ZH = original
    If Not attempted Then
        attempted = True
        Set translations = CreateObject("Scripting.Dictionary")
        translations.CompareMode = 0
        Set stream = CreateObject("ADODB.Stream")
        stream.Type = 2
        stream.Charset = "utf-8"
        stream.Open
        stream.LoadFromFile App.Path & "\zh-CN.tsv"
        rows = Split(Replace(stream.ReadText, vbCr, ""), vbLf)
        stream.Close
        For Each row In rows
            fields = Split(CStr(row), vbTab)
            If UBound(fields) = 1 Then translations(CStr(fields(0))) = CStr(fields(1))
        Next row
        Set fallbackCache = CreateObject("Scripting.Dictionary")
        fallbackCache.CompareMode = 0
    End If
    If Not translations Is Nothing Then
        If translations.Exists(original) Then
            ZH = translations(original)
        ElseIf original Like "*[A-Za-z]*" Then
            If Not fallbackCache Is Nothing Then
                If fallbackCache.Exists(original) Then
                    ZH = fallbackCache(original)
                    Exit Function
                End If
            End If
            For Each key In translations.Keys
                If Len(CStr(key)) >= 10 And Len(CStr(key)) > Len(best) And Len(original) > Len(CStr(key)) Then
                    If Left$(original, Len(CStr(key))) = CStr(key) Then best = CStr(key)
                End If
            Next key
            If Len(best) > 0 Then ZH = translations(best) & Mid$(original, Len(best) + 1)
            If Not fallbackCache Is Nothing Then
                If fallbackCache.Count >= 512 Then fallbackCache.RemoveAll
                fallbackCache(original) = ZH
            End If
        End If
    End If
fallback:
End Function

Public Sub LocalizeControls(ByVal host As Object)
    On Error Resume Next
    Dim control As Object, i As Long
    host.Caption = ZH(host.Caption)
    For Each control In host.Controls
        control.Caption = ZH(control.Caption)
        control.ToolTipText = ZH(control.ToolTipText)
        control.ButtonCaption = ZH(control.ButtonCaption)
        If TypeName(control) = "SSTab" Then
            For i = 0 To control.Tabs - 1
                control.TabCaption(i) = ZH(control.TabCaption(i))
            Next i
        End If
    Next control
End Sub

Public Function MatchesLegacyLayout(ByVal process As Long, ByVal address As Long, ByRef expected() As Byte, ByRef injected() As Byte) As Boolean
    On Error GoTo unsupported
    Dim actual(4) As Byte, count As Long, i As Long
    Dim originalMatches As Boolean, injectedMatches As Boolean
    If process = 0 Or address = 0 Then Exit Function
    If ReadProcessMemory(process, address, actual(0), 5&, count) = 0 Then Exit Function
    If count <> 5 Then Exit Function
    originalMatches = True
    injectedMatches = True
    For i = 0 To 4
        If actual(i) <> expected(i) Then originalMatches = False
        If actual(i) <> injected(i) Then injectedMatches = False
    Next i
    MatchesLegacyLayout = originalMatches Or injectedMatches
unsupported:
End Function
