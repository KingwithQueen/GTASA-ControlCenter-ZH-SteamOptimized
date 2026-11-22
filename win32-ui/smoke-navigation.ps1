<#
.SYNOPSIS
Checks mouse and keyboard navigation in a running instance of the Win32 prototype.
.EXAMPLE
powershell -File .\win32-ui\smoke-navigation.ps1
.EXAMPLE
pwsh -File .\win32-ui\smoke-navigation.ps1 -ExePath C:\build\GTASAControlCenterUI.exe -TestGlobalShortcuts
.NOTES
-TestGlobalShortcuts briefly brings the test window into the foreground and tests Ctrl+Tab and Shift+Ctrl+Tab.
#>
[CmdletBinding()]
param(
    [string] $ExePath,
    [switch] $TestGlobalShortcuts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($ExePath)) {
    $ExePath = Join-Path $PSScriptRoot '..\.work\win32-ui\Release\GTASAControlCenterUI.exe'
}
$exe = (Resolve-Path -LiteralPath $ExePath).ProviderPath
Add-Type -AssemblyName Accessibility

if (-not ('GtasaUiSmoke.Native' -as [type])) {
    $compilerArguments = @{ ReferencedAssemblies = [Accessibility.IAccessible].Assembly.Location }
    if ($PSVersionTable.PSEdition -eq 'Core') {
        # The inbox interop assembly references .NET Framework identities on both shells.
        $compilerArguments.CompilerOptions = '/nowarn:1701'
    }
    Add-Type @compilerArguments -TypeDefinition @'
using Accessibility;
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace GtasaUiSmoke
{
    public static class Native
    {
        private const uint ObjIdClient = 0xFFFFFFFC;
        private const uint TcmGetItemCount = 0x1304;
        private const uint TcmGetCurSel = 0x130B;
        private const uint LvmGetItemCount = 0x1004;
        private const uint SmtoBlockAbortIfHungErrorOnExit = 0x23;
        private const uint CwpSkipInvisibleAndDisabled = 0x03;
        private const uint KeyUp = 2;
        private delegate bool EnumerateWindow(IntPtr window, IntPtr state);

        public struct TabItem
        {
            public string Name;
            public int Left, Top, Width, Height;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Point
        {
            public int X, Y;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Rect
        {
            public int Left, Top, Right, Bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct KeyboardInput
        {
            public ushort Key, Scan;
            public uint Flags, Time;
            public IntPtr ExtraInfo;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct MouseInput
        {
            public int X, Y;
            public uint Data, Flags, Time;
            public IntPtr ExtraInfo;
        }

        [StructLayout(LayoutKind.Explicit)]
        private struct InputData
        {
            [FieldOffset(0)] public KeyboardInput Keyboard;
            [FieldOffset(0)] public MouseInput Mouse;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct Input
        {
            public uint Type;
            public InputData Data;
        }

        [DllImport("user32.dll")]
        public static extern IntPtr GetDlgItem(IntPtr parent, int id);
        [DllImport("user32.dll")]
        public static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr window, EnumerateWindow callback, IntPtr state);
        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern int GetClassName(IntPtr window, StringBuilder name, int capacity);
        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool ScreenToClient(IntPtr window, ref Point point);
        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool GetClientRect(IntPtr window, out Rect rect);
        [DllImport("user32.dll")]
        private static extern int MapWindowPoints(IntPtr from, IntPtr to, ref Point points, uint count);
        [DllImport("user32.dll")]
        private static extern IntPtr ChildWindowFromPointEx(IntPtr parent, Point point, uint flags);
        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
        [DllImport("user32.dll", SetLastError = true)]
        private static extern IntPtr SendMessageTimeout(IntPtr window, uint message, IntPtr wParam,
            IntPtr lParam, uint flags, uint timeoutMs, out IntPtr result);
        [DllImport("user32.dll")]
        private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
        [DllImport("user32.dll")]
        private static extern IntPtr GetWindowDpiAwarenessContext(IntPtr window);
        [DllImport("user32.dll")]
        public static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint SendInput(uint count, Input[] inputs, int size);
        [DllImport("oleacc.dll")]
        private static extern int AccessibleObjectFromWindow(IntPtr window, uint objectId, ref Guid iid,
            [MarshalAs(UnmanagedType.Interface)] out IAccessible accessible);

        public static string WindowClass(IntPtr window)
        {
            var name = new StringBuilder(256);
            if (GetClassName(window, name, name.Capacity) == 0)
                throw Win32Failure("Could not read the window class.", Marshal.GetLastWin32Error());
            return name.ToString();
        }

        public static int VisibleDescendants(IntPtr window)
        {
            int count = 0;
            EnumChildWindows(window, (child, state) => {
                if (IsWindowVisible(child)) count++;
                return true;
            }, IntPtr.Zero);
            return count;
        }

        public static bool IsPageOnTopAtCenter(IntPtr parent, IntPtr page)
        {
            Rect rect;
            if (!GetClientRect(page, out rect))
                throw Win32Failure("Could not read the page bounds.", Marshal.GetLastWin32Error());
            var center = new Point { X = (rect.Left + rect.Right) / 2, Y = (rect.Top + rect.Bottom) / 2 };
            MapWindowPoints(page, parent, ref center, 1);
            return ChildWindowFromPointEx(parent, center, CwpSkipInvisibleAndDisabled) == page;
        }

        public static int TabCount(IntPtr window) { return ReadControlInt(window, TcmGetItemCount); }
        public static int SelectedTab(IntPtr window) { return ReadControlInt(window, TcmGetCurSel); }
        public static int ListCount(IntPtr window) { return ReadControlInt(window, LvmGetItemCount); }

        public static TabItem[] TabItems(IntPtr window)
        {
            IntPtr previous = EnterWindowDpi(window);
            IAccessible accessible = null;
            try
            {
                Guid iid = typeof(IAccessible).GUID;
                Marshal.ThrowExceptionForHR(AccessibleObjectFromWindow(window, ObjIdClient, ref iid, out accessible));
                var items = new TabItem[accessible.accChildCount];
                for (int index = 0; index < items.Length; index++)
                {
                    int childId = index + 1;
                    items[index].Name = accessible.get_accName(childId);
                    accessible.accLocation(out items[index].Left, out items[index].Top,
                        out items[index].Width, out items[index].Height, childId);
                }
                return items;
            }
            finally
            {
                if (accessible != null) Marshal.ReleaseComObject(accessible);
                if (previous != IntPtr.Zero) SetThreadDpiAwarenessContext(previous);
            }
        }

        public static void Click(IntPtr tab, int screenX, int screenY)
        {
            var point = new Point { X = screenX, Y = screenY };
            IntPtr previous = EnterWindowDpi(tab);
            try
            {
                if (!ScreenToClient(tab, ref point))
                    throw Win32Failure("Could not map the tab click.", Marshal.GetLastWin32Error());
                int position = (point.X & 0xFFFF) | ((point.Y & 0xFFFF) << 16);
                Post(tab, 0x0201, 1, position);
                Post(tab, 0x0202, 0, position);
            }
            finally
            {
                if (previous != IntPtr.Zero) SetThreadDpiAwarenessContext(previous);
            }
        }

        public static void Arrow(IntPtr tab, bool right)
        {
            int key = right ? 0x27 : 0x25;
            int scanCode = right ? 0x4D : 0x4B;
            int flags = 0x01000001 | (scanCode << 16);
            Post(tab, 0x0100, key, flags);
            Post(tab, 0x0101, key, flags | unchecked((int)0xC0000000));
        }

        public static void Close(IntPtr window) { Post(window, 0x0010, 0, 0); }

        public static void CtrlTab(bool reverse)
        {
            const ushort Tab = 0x09, Shift = 0x10, Control = 0x11;
            Input[] inputs = reverse
                ? new[] { Key(Control), Key(Shift), Key(Tab), Key(Tab, true), Key(Shift, true), Key(Control, true) }
                : new[] { Key(Control), Key(Tab), Key(Tab, true), Key(Control, true) };

            uint sent = SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(Input)));
            if (sent != inputs.Length)
            {
                int error = Marshal.GetLastWin32Error();
                var release = new[] { Key(Tab, true), Key(Shift, true), Key(Control, true) };
                SendInput((uint)release.Length, release, Marshal.SizeOf(typeof(Input)));
                throw Win32Failure("Could not send the complete Ctrl+Tab shortcut.", error);
            }
        }

        private static Input Key(ushort key, bool released = false)
        {
            return new Input {
                Type = 1,
                Data = new InputData { Keyboard = new KeyboardInput { Key = key, Flags = released ? KeyUp : 0 } }
            };
        }

        private static int ReadControlInt(IntPtr window, uint message)
        {
            IntPtr result;
            if (SendMessageTimeout(window, message, IntPtr.Zero, IntPtr.Zero,
                SmtoBlockAbortIfHungErrorOnExit, 1000, out result) == IntPtr.Zero)
                throw Win32Failure("The control did not respond.", Marshal.GetLastWin32Error());
            return result.ToInt32();
        }

        private static void Post(IntPtr window, uint message, int wParam, int lParam)
        {
            if (!PostMessage(window, message, new IntPtr(wParam), new IntPtr(lParam)))
                throw Win32Failure("Could not post the window message.", Marshal.GetLastWin32Error());
        }

        private static InvalidOperationException Win32Failure(string message, int error)
        {
            return new InvalidOperationException(message + " Win32 error " + error + ".");
        }

        private static IntPtr EnterWindowDpi(IntPtr window)
        {
            try { return SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(window)); }
            catch (EntryPointNotFoundException) { return IntPtr.Zero; }
        }
    }
}
'@
}

function Get-Page([int] $Index) {
    [GtasaUiSmoke.Native]::GetDlgItem($mainWindow, 200 + $Index)
}

function Assert-SelectedPage([int] $Expected, [string] $InputDescription) {
    $timeout = [Diagnostics.Stopwatch]::StartNew()
    do {
        $selected = [GtasaUiSmoke.Native]::SelectedTab($tab)
        $page = Get-Page $Expected
        if ($selected -eq $Expected -and [GtasaUiSmoke.Native]::IsWindowVisible($page)) {
            break
        }
        Start-Sleep -Milliseconds 25
    } while ($timeout.ElapsedMilliseconds -lt 2000)

    if ($selected -ne $Expected -or -not [GtasaUiSmoke.Native]::IsWindowVisible($page)) {
        throw "$InputDescription did not show tab $Expected (current selection $selected)."
    }
    if (-not [GtasaUiSmoke.Native]::IsPageOnTopAtCenter($mainWindow, $page)) {
        throw "$InputDescription left the center of active page $Expected covered by another window."
    }

    for ($index = 0; $index -lt $tabCount; $index++) {
        $candidate = Get-Page $index
        if ($index -ne $Expected -and [GtasaUiSmoke.Native]::IsWindowVisible($candidate)) {
            throw "$InputDescription left inactive page $index visible."
        }
    }

    if ($visitedPages.ContainsKey($Expected) -and $visitedPages[$Expected] -ne $page) {
        throw "$InputDescription recreated page $Expected."
    }
    $visitedPages[$Expected] = $page
    $visibleControls[$Expected] = [GtasaUiSmoke.Native]::VisibleDescendants($page)
    if ($visibleControls[$Expected] -eq 0) {
        throw "$InputDescription displayed an empty page for tab $Expected."
    }

    if ($expectedListRows.ContainsKey($Expected)) {
        $list = [GtasaUiSmoke.Native]::GetDlgItem($page, 1000)
        if (-not [GtasaUiSmoke.Native]::IsWindowVisible($list)) {
            throw "$InputDescription did not display the list for tab $Expected."
        }
        $rowCount = [GtasaUiSmoke.Native]::ListCount($list)
        if ($rowCount -ne $expectedListRows[$Expected]) {
            throw "$InputDescription displayed $rowCount rows for tab $Expected, expected $($expectedListRows[$Expected])."
        }
    }
}

function Select-TabByClick([int] $Index) {
    $bounds = $tabItems[$Index]
    if ($bounds.Width -le 0 -or $bounds.Height -le 0) {
        throw "Tab $Index has no clickable bounds."
    }
    $x = [int]($bounds.Left + $bounds.Width / 2)
    $y = [int]($bounds.Top + $bounds.Height / 2)
    [GtasaUiSmoke.Native]::Click($tab, $x, $y)
    Assert-SelectedPage $Index "Mouse click on tab $Index"
}

$process = Start-Process -FilePath $exe -PassThru
$mainWindow = [IntPtr]::Zero
try {
    $startup = [Diagnostics.Stopwatch]::StartNew()
    do {
        $process.Refresh()
        if ($process.HasExited) {
            throw "The prototype exited before showing its window (exit code $($process.ExitCode))."
        }
        $mainWindow = $process.MainWindowHandle
        if ($mainWindow -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 50
    } while ($startup.ElapsedMilliseconds -lt 10000)

    if ($mainWindow -eq [IntPtr]::Zero) { throw 'The prototype did not show a window within ten seconds.' }
    if ([GtasaUiSmoke.Native]::WindowClass($mainWindow) -ne 'GTASAControlCenterPrototype') {
        throw 'The process showed a window with an unexpected class name.'
    }

    $tab = [GtasaUiSmoke.Native]::GetDlgItem($mainWindow, 100)
    $status = [GtasaUiSmoke.Native]::GetDlgItem($mainWindow, 101)
    if ($tab -eq [IntPtr]::Zero -or -not [GtasaUiSmoke.Native]::IsWindowVisible($status)) {
        throw 'The main window is missing its tab control or status bar.'
    }

    $tabCount = [GtasaUiSmoke.Native]::TabCount($tab)
    if ($tabCount -ne 6) { throw "Expected six tabs but found $tabCount." }
    $tabItems = [GtasaUiSmoke.Native]::TabItems($tab)
    if ($tabItems.Count -ne $tabCount) { throw 'The accessibility provider did not return all tab items.' }

    $visitedPages = @{}
    $visibleControls = @{}
    $expectedListRows = @{ 2 = 5; 3 = 130; 4 = 503; 5 = 69 }
    Assert-SelectedPage 0 'Startup'

    for ($index = 1; $index -lt $tabCount; $index++) { Select-TabByClick $index }
    for ($index = $tabCount - 2; $index -ge 0; $index--) { Select-TabByClick $index }

    [GtasaUiSmoke.Native]::Arrow($tab, $true)
    Assert-SelectedPage 1 'Right arrow'
    [GtasaUiSmoke.Native]::Arrow($tab, $false)
    Assert-SelectedPage 0 'Left arrow'

    if ($TestGlobalShortcuts) {
        [void][GtasaUiSmoke.Native]::SetForegroundWindow($mainWindow)
        $focusTimeout = [Diagnostics.Stopwatch]::StartNew()
        while ([GtasaUiSmoke.Native]::GetForegroundWindow() -ne $mainWindow -and
            $focusTimeout.ElapsedMilliseconds -lt 1000) {
            Start-Sleep -Milliseconds 25
        }
        if ([GtasaUiSmoke.Native]::GetForegroundWindow() -ne $mainWindow) {
            throw 'The test window could not take foreground focus for Ctrl+Tab testing.'
        }

        [GtasaUiSmoke.Native]::CtrlTab($true)
        Assert-SelectedPage ($tabCount - 1) 'Shift+Ctrl+Tab from the first tab'
        [GtasaUiSmoke.Native]::CtrlTab($false)
        Assert-SelectedPage 0 'Ctrl+Tab from the last tab'
    }

    for ($index = 0; $index -lt $tabCount; $index++) {
        $details = '{0} visible descendant windows' -f $visibleControls[$index]
        if ($expectedListRows.ContainsKey($index)) { $details += ', {0} list rows' -f $expectedListRows[$index] }
        'Tab {0}  {1} ({2})' -f $index, $tabItems[$index].Name, $details
    }
    'PASS: Mouse and arrow-key navigation across six pages with cached page windows.'
    if ($TestGlobalShortcuts) { 'PASS: Ctrl+Tab and Shift+Ctrl+Tab wrap between the first and last pages.' }
}
finally {
    $process.Refresh()
    if (-not $process.HasExited) {
        if ($mainWindow -ne [IntPtr]::Zero) {
            try { [GtasaUiSmoke.Native]::Close($mainWindow) }
            catch [InvalidOperationException] { }
        }
        if (-not $process.WaitForExit(2000)) { $process.Kill() }
    }
    $process.Dispose()
}
