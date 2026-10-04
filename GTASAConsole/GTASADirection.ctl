VERSION 5.00
Begin VB.UserControl GTASADirection 
   ClientHeight    =   1830
   ClientLeft      =   0
   ClientTop       =   0
   ClientWidth     =   1830
   LockControls    =   -1  'True
   ScaleHeight     =   1830
   ScaleWidth      =   1830
   Begin VB.VScrollBar scrKickStart 
      Height          =   1560
      Left            =   1575
      Max             =   0
      Min             =   -2000
      TabIndex        =   11
      TabStop         =   0   'False
      Top             =   225
      Visible         =   0   'False
      Width           =   225
   End
   Begin VB.PictureBox picFlipContainer 
      Appearance      =   0  'Flat
      BackColor       =   &H80000004&
      ForeColor       =   &H80000008&
      Height          =   1560
      Left            =   15
      ScaleHeight     =   1530
      ScaleWidth      =   1530
      TabIndex        =   0
      TabStop         =   0   'False
      Top             =   225
      Width           =   1560
      Begin VB.CommandButton cmdDirection 
         Caption         =   "Set Direction"
         Height          =   555
         Left            =   255
         TabIndex        =   9
         TabStop         =   0   'False
         Top             =   465
         Width           =   990
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   1
         Left            =   1095
         TabIndex        =   8
         TabStop         =   0   'False
         ToolTipText     =   "Nort-East"
         Top             =   210
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   2
         Left            =   1305
         TabIndex        =   7
         TabStop         =   0   'False
         ToolTipText     =   "East"
         Top             =   675
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   3
         Left            =   1125
         TabIndex        =   6
         TabStop         =   0   'False
         ToolTipText     =   "South-East"
         Top             =   1110
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   5
         Left            =   180
         TabIndex        =   5
         TabStop         =   0   'False
         ToolTipText     =   "South-West"
         Top             =   1095
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   6
         Left            =   0
         TabIndex        =   4
         TabStop         =   0   'False
         ToolTipText     =   "West"
         Top             =   645
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   7
         Left            =   225
         TabIndex        =   3
         TabStop         =   0   'False
         ToolTipText     =   "North-West"
         Top             =   195
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   0
         Left            =   675
         TabIndex        =   2
         TabStop         =   0   'False
         ToolTipText     =   "North"
         Top             =   -15
         Value           =   -1  'True
         Width           =   195
      End
      Begin VB.OptionButton optFlipDirection 
         Height          =   195
         Index           =   4
         Left            =   645
         TabIndex        =   1
         TabStop         =   0   'False
         ToolTipText     =   "South"
         Top             =   1305
         Width           =   195
      End
      Begin VB.Shape shpBorder 
         Height          =   1515
         Left            =   0
         Shape           =   3  'Circle
         Top             =   0
         Width           =   1515
      End
   End
   Begin VB.Label lblCaption 
      AutoSize        =   -1  'True
      Caption         =   "Car Direction:"
      Height          =   195
      Left            =   0
      TabIndex        =   10
      Top             =   0
      Width           =   1785
   End
End
Attribute VB_Name = "GTASADirection"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = True
Attribute VB_PredeclaredId = False
Attribute VB_Exposed = False
Option Explicit
Private Declare Function SetParentWin Lib "user32" Alias "SetParent" (ByVal hWndChild As Long, ByVal hWndNewParent As Long) As Long
Private Declare Function MoveWindowNative Lib "user32" Alias "MoveWindow" (ByVal hWnd As Long, ByVal X As Long, ByVal Y As Long, ByVal nWidth As Long, ByVal nHeight As Long, ByVal bRepaint As Long) As Long
Private Declare Function ShowWindowNative Lib "user32" Alias "ShowWindow" (ByVal hWnd As Long, ByVal nCmdShow As Long) As Long
Private Declare Function EnableWindowNative Lib "user32" Alias "EnableWindow" (ByVal hWnd As Long, ByVal bEnable As Long) As Long
'Default Property Values:
Const m_def_Caption = "Car Direction:"
Const m_def_Direction = 0
Const m_def_HasScroller = False
'Property Variables:
Dim m_Caption As String
Dim m_Direction As Integer
Dim m_HasScroller As Boolean
Dim m_IsNativeAttached As Boolean
Private m_NativeLayoutValid As Boolean
Private m_NativeParent As Long
Private m_NativeLeft As Long, m_NativeTop As Long
Private m_NativeTwipsX As Long, m_NativeTwipsY As Long
'Event Declarations:
Event ButtonClick() 'MappingInfo=cmdDirection,cmdDirection,-1,Click
Attribute ButtonClick.VB_Description = "Occurs when the user presses and then releases a mouse button over an object."

Public Sub PlaceOnSSTab(ByVal ParentHwnd As Long, ByVal LeftTwips As Long, ByVal TopTwips As Long, ByVal ShowIt As Boolean)
On Error Resume Next
    Dim px As Long, py As Long
    Dim layoutOK As Boolean
    If Not m_IsNativeAttached Or m_NativeParent <> ParentHwnd Then
        m_NativeLayoutValid = False
        SetParentWin picFlipContainer.hWnd, ParentHwnd
        SetParentWin scrKickStart.hWnd, ParentHwnd
        m_IsNativeAttached = True
        m_NativeParent = ParentHwnd
    End If
    If ShowIt Then
        px = LeftTwips \ Screen.TwipsPerPixelX
        py = TopTwips \ Screen.TwipsPerPixelY
        If Not m_NativeLayoutValid Or m_NativeLeft <> px Or m_NativeTop <> py Or m_NativeTwipsX <> Screen.TwipsPerPixelX Or m_NativeTwipsY <> Screen.TwipsPerPixelY Then
            layoutOK = True
            If MoveWindowNative(picFlipContainer.hWnd, px + 15 \ Screen.TwipsPerPixelX, py + 225 \ Screen.TwipsPerPixelY, 1560 \ Screen.TwipsPerPixelX, 1560 \ Screen.TwipsPerPixelY, 1) = 0 Then layoutOK = False
            If MoveWindowNative(scrKickStart.hWnd, px + 1575 \ Screen.TwipsPerPixelX, py + 225 \ Screen.TwipsPerPixelY, 225 \ Screen.TwipsPerPixelX, 1560 \ Screen.TwipsPerPixelY, 1) = 0 Then layoutOK = False
            m_NativeLeft = px: m_NativeTop = py
            m_NativeTwipsX = Screen.TwipsPerPixelX: m_NativeTwipsY = Screen.TwipsPerPixelY
            m_NativeLayoutValid = layoutOK
        End If
    End If
    EnableWindowNative picFlipContainer.hWnd, 1
    EnableWindowNative scrKickStart.hWnd, 1
    ShowWindowNative picFlipContainer.hWnd, IIf(ShowIt, 5, 0)
    ShowWindowNative scrKickStart.hWnd, IIf(ShowIt And m_HasScroller, 5, 0)
End Sub

Private Sub optFlipDirection_Click(Index As Integer)
On Error Resume Next
    '0:North       '1:North-East     '2:East
    '3:South-East  '4:South          '5:South-West
    '6:West        '7:North-West
    m_Direction = Index
End Sub

Private Sub SetLabelCaption()
On Error Resume Next
    If m_HasScroller Then
        lblCaption.Caption = Replace(m_Caption, ":", "") & " (" & Abs(scrKickStart.Value / 10) & " %):"
    Else
        lblCaption.Caption = m_Caption
    End If
End Sub

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MemberInfo=7,0,0,0
Public Property Get Direction() As Integer
    Direction = m_Direction
End Property

Public Property Let Direction(ByVal New_Direction As Integer)
    m_Direction = New_Direction
    optFlipDirection(New_Direction).Value = True
    PropertyChanged "Direction"
End Property

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MappingInfo=cmdDirection,cmdDirection,-1,Caption
Public Property Get ButtonCaption() As String
Attribute ButtonCaption.VB_Description = "Returns/sets the text displayed in an object's title bar or below an object's icon."
    ButtonCaption = cmdDirection.Caption
End Property

Public Property Let ButtonCaption(ByVal New_ButtonCaption As String)
    cmdDirection.Caption = New_ButtonCaption
    PropertyChanged "ButtonCaption"
End Property

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MemberInfo=0,0,0,0
Public Property Get HasScroller() As Boolean
    HasScroller = m_HasScroller
End Property

Public Property Let HasScroller(ByVal New_HasScroller As Boolean)
    m_HasScroller = New_HasScroller
    scrKickStart.Visible = New_HasScroller
    SetLabelCaption
    PropertyChanged "HasScroller"
End Property

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MappingInfo=scrKickStart,scrKickStart,-1,Max
Public Property Get ScrollerMax() As Integer
Attribute ScrollerMax.VB_Description = "Returns/sets a scroll bar position's maximum Value property setting."
    ScrollerMax = scrKickStart.max
End Property

Public Property Let ScrollerMax(ByVal New_ScrollerMax As Integer)
    scrKickStart.max = New_ScrollerMax
    SetLabelCaption
    PropertyChanged "ScrollerMax"
End Property

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MappingInfo=scrKickStart,scrKickStart,-1,Min
Public Property Get ScrollerMin() As Integer
Attribute ScrollerMin.VB_Description = "Returns/sets a scroll bar position's maximum Value property setting."
    ScrollerMin = scrKickStart.min
End Property

Public Property Let ScrollerMin(ByVal New_ScrollerMin As Integer)
    scrKickStart.min = New_ScrollerMin
    SetLabelCaption
    PropertyChanged "ScrollerMin"
End Property

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MappingInfo=scrKickStart,scrKickStart,-1,Value
Public Property Get ScrollerVal() As Integer
Attribute ScrollerVal.VB_Description = "Returns/sets the value of an object."
    ScrollerVal = scrKickStart.Value
End Property

Public Property Let ScrollerVal(ByVal New_ScrollerVal As Integer)
    scrKickStart.Value = New_ScrollerVal
    SetLabelCaption
    PropertyChanged "ScrollerVal"
End Property

Private Sub cmdDirection_Click()
    RaiseEvent ButtonClick
End Sub

Private Sub scrKickStart_Change()
    SetLabelCaption
End Sub

'Initialize Properties for User Control
Private Sub UserControl_InitProperties()
    m_Direction = m_def_Direction
    m_HasScroller = m_def_HasScroller
    m_Caption = m_def_Caption
End Sub

'Load property values from storage
Private Sub UserControl_ReadProperties(PropBag As PropertyBag)

    m_Direction = PropBag.ReadProperty("Direction", m_def_Direction)
    cmdDirection.Caption = PropBag.ReadProperty("ButtonCaption", ZH("Set Direction"))
    m_HasScroller = PropBag.ReadProperty("HasScroller", m_def_HasScroller)
    scrKickStart.max = PropBag.ReadProperty("ScrollerMax", 0)
    scrKickStart.min = PropBag.ReadProperty("ScrollerMin", -2000)
    scrKickStart.Value = PropBag.ReadProperty("ScrollerVal", 0)
    m_Caption = PropBag.ReadProperty("Caption", m_def_Caption)
    scrKickStart.Visible = m_HasScroller
    SetLabelCaption
    LocalizeControls UserControl
End Sub

'Write property values to storage
Private Sub UserControl_WriteProperties(PropBag As PropertyBag)

    Call PropBag.WriteProperty("Direction", m_Direction, m_def_Direction)
    Call PropBag.WriteProperty("ButtonCaption", cmdDirection.Caption, ZH("Set Direction"))
    Call PropBag.WriteProperty("HasScroller", m_HasScroller, m_def_HasScroller)
    Call PropBag.WriteProperty("ScrollerMax", scrKickStart.max, 0)
    Call PropBag.WriteProperty("ScrollerMin", scrKickStart.min, -2000)
    Call PropBag.WriteProperty("ScrollerVal", scrKickStart.Value, 0)
    Call PropBag.WriteProperty("Caption", m_Caption, m_def_Caption)
End Sub

'WARNING! DO NOT REMOVE OR MODIFY THE FOLLOWING COMMENTED LINES!
'MemberInfo=13,0,0,Car Direction:
Public Property Get Caption() As String
Attribute Caption.VB_Description = "Returns/sets the text displayed in an object's title bar or below an object's icon."
    Caption = m_Caption
End Property

Public Property Let Caption(ByVal New_Caption As String)
    m_Caption = New_Caption
    SetLabelCaption
    PropertyChanged "Caption"
End Property

