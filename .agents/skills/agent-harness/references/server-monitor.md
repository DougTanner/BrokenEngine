# Agent Harness Server Monitor Recipe

[Back to the command reference](command-reference.md)

### Server Monitor window capture and click

The server has no visible window. The separate Server Monitor is a plain Win32
GDI window; client `screenshot`, `describe_ui`, and `click` cannot drive it.
Build it with `/compile -Target ServerMonitor`, then start it from the adopted
worktree root:

```powershell
$monitor = Start-Process -FilePath 'Tools\ServerMonitor\Platforms\VisualStudio2026\Output\ServerMonitor.exe' -WindowStyle Minimized -PassThru
```

Retain `$monitor.Id` and pass it as `-MonitorPid` to the helper below. A harness
server's app-data root holds no secret file unless the scenario writes one.
Stop the monitor with `Stop-Process -Id <pid>` after capture. Copy the complete
block below verbatim into the ignored `Temp\Capture-ServerMonitor.ps1` under
the adopted worktree. Keep PID ownership; do not use a process-name search or
add a tracked helper.
```powershell
param(
	[Parameter(Mandatory = $true)]
	[ValidateRange(1, [int]::MaxValue)]
	[int] $MonitorPid,

	[ValidateSet('Map', 'Profile', 'ProfileCopy')]
	[string] $Target = 'Profile',

	[string] $BeforeCapturePath,
	[string] $AfterCapturePath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class AgentServerMonitorNative
{
	public const int GWL_EXSTYLE = -20;
	public const long WS_EX_TOPMOST = 0x00000008L;
	public const uint GW_OWNER = 4;
	public const uint GW_HWNDPREV = 3;
	public const int SW_HIDE = 0;
	public const int SW_MINIMIZE = 6;
	public const int SW_RESTORE = 9;
	public const int SW_SHOWNOACTIVATE = 4;
	public const uint SWP_NOSIZE = 0x0001;
	public const uint SWP_NOMOVE = 0x0002;
	public const uint SWP_NOACTIVATE = 0x0010;
	public const uint SWP_SHOWWINDOW = 0x0040;
	public const uint RDW_INVALIDATE = 0x0001;
	public const uint RDW_ERASE = 0x0004;
	public const uint RDW_UPDATENOW = 0x0100;
	public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
	public const uint MOUSEEVENTF_LEFTUP = 0x0004;
	public const uint PW_CLIENTONLY = 0x00000001;
	public const uint PW_RENDERFULLCONTENT = 0x00000002;
	public static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
	public static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);

	[StructLayout(LayoutKind.Sequential)]
	public struct POINT
	{
		public int X;
		public int Y;
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct RECT
	{
		public int left;
		public int top;
		public int right;
		public int bottom;
	}

	public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

	[DllImport("user32.dll")]
	public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool IsProcessDPIAware();

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool SetProcessDPIAware();

	[DllImport("user32.dll")]
	public static extern IntPtr GetWindow(IntPtr hWnd, uint uCmd);

	[DllImport("user32.dll")]
	public static extern IntPtr GetParent(IntPtr hWnd);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool IsWindow(IntPtr hWnd);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool IsWindowVisible(IntPtr hWnd);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool IsIconic(IntPtr hWnd);

	[DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW", SetLastError = true)]
	public static extern IntPtr GetWindowLongPtr(IntPtr hWnd, int nIndex);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool GetCursorPos(out POINT lpPoint);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool SetCursorPos(int X, int Y);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool BringWindowToTop(IntPtr hWnd);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool SetForegroundWindow(IntPtr hWnd);

	[DllImport("user32.dll")]
	public static extern IntPtr GetForegroundWindow();

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool RedrawWindow(IntPtr hWnd, IntPtr lprcUpdate, IntPtr hrgnUpdate, uint flags);

	[DllImport("user32.dll")]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool UpdateWindow(IntPtr hWnd);

	[DllImport("user32.dll", SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);

	[DllImport("user32.dll")]
	public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

	public static IntPtr FindTopLevelWindow(int processId)
	{
		IntPtr found = IntPtr.Zero;
		EnumWindows((hWnd, lParam) =>
		{
			uint ownerProcessId;
			if (GetWindowThreadProcessId(hWnd, out ownerProcessId) == 0 || ownerProcessId != (uint)processId)
			{
				return true;
			}

			if (GetParent(hWnd) != IntPtr.Zero || GetWindow(hWnd, GW_OWNER) != IntPtr.Zero)
			{
				return true;
			}

			found = hWnd;
			return false;
		}, IntPtr.Zero);
		return found;
	}
}
'@

if (-not [AgentServerMonitorNative]::IsProcessDPIAware())
{
	if (-not [AgentServerMonitorNative]::SetProcessDPIAware())
	{
		throw 'SetProcessDPIAware failed for the server-monitor capture helper.'
	}
}

if ([string]::IsNullOrWhiteSpace($BeforeCapturePath))
{
	$BeforeCapturePath = Join-Path $PSScriptRoot ('server-monitor-before-{0}.png' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
}
if ([string]::IsNullOrWhiteSpace($AfterCapturePath))
{
	$AfterCapturePath = Join-Path $PSScriptRoot ('server-monitor-after-{0}.png' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
}

function Format-Handle([IntPtr] $Handle)
{
	if ($Handle -eq [IntPtr]::Zero)
	{
		return '0x0'
	}
	return ('0x{0:X}' -f $Handle.ToInt64())
}

function Get-ServerWindowState([IntPtr] $Hwnd)
{
	$cursor = [AgentServerMonitorNative+POINT]::new()
	if (-not [AgentServerMonitorNative]::GetCursorPos([ref] $cursor))
	{
		throw 'GetCursorPos failed while recording server-monitor state.'
	}

	$windowRect = [AgentServerMonitorNative+RECT]::new()
	if (-not [AgentServerMonitorNative]::GetWindowRect($Hwnd, [ref] $windowRect))
	{
		throw 'GetWindowRect failed while recording server-monitor state.'
	}

	$clientRect = [AgentServerMonitorNative+RECT]::new()
	if (-not [AgentServerMonitorNative]::GetClientRect($Hwnd, [ref] $clientRect))
	{
		throw 'GetClientRect failed while recording server-monitor state.'
	}

	$extendedStyle = [AgentServerMonitorNative]::GetWindowLongPtr($Hwnd, [AgentServerMonitorNative]::GWL_EXSTYLE).ToInt64()
	[pscustomobject]@{
		CursorX = $cursor.X
		CursorY = $cursor.Y
		Visible = [bool][AgentServerMonitorNative]::IsWindowVisible($Hwnd)
		Minimized = [bool][AgentServerMonitorNative]::IsIconic($Hwnd)
		Topmost = (($extendedStyle -band [AgentServerMonitorNative]::WS_EX_TOPMOST) -ne 0)
		Foreground = [AgentServerMonitorNative]::GetForegroundWindow()
		Previous = [AgentServerMonitorNative]::GetWindow($Hwnd, [AgentServerMonitorNative]::GW_HWNDPREV)
		WindowRect = $windowRect
		ClientRect = $clientRect
	}
}

function Write-ServerWindowState([string] $Label, $State)
{
	'{0}: cursor=({1},{2}) visible={3} minimized={4} topmost={5} foreground={6} previous={7} window=({8},{9})..({10},{11}) client=({12},{13})' -f `
		$Label, $State.CursorX, $State.CursorY, $State.Visible, $State.Minimized, $State.Topmost, `
		(Format-Handle $State.Foreground), (Format-Handle $State.Previous), `
		$State.WindowRect.left, $State.WindowRect.top, $State.WindowRect.right, $State.WindowRect.bottom, `
		$State.ClientRect.left, $State.ClientRect.top
}

function Refresh-ServerWindow([IntPtr] $Hwnd)
{
	[AgentServerMonitorNative]::ShowWindow($Hwnd, [AgentServerMonitorNative]::SW_RESTORE) | Out-Null
	[AgentServerMonitorNative]::ShowWindow($Hwnd, [AgentServerMonitorNative]::SW_SHOWNOACTIVATE) | Out-Null
	$raiseFlags = [uint32]([AgentServerMonitorNative]::SWP_NOMOVE -bor [AgentServerMonitorNative]::SWP_NOSIZE -bor [AgentServerMonitorNative]::SWP_NOACTIVATE -bor [AgentServerMonitorNative]::SWP_SHOWWINDOW)
	[AgentServerMonitorNative]::SetWindowPos($Hwnd, [AgentServerMonitorNative]::HWND_TOPMOST, 0, 0, 0, 0, $raiseFlags) | Out-Null
	[AgentServerMonitorNative]::BringWindowToTop($Hwnd) | Out-Null
	[AgentServerMonitorNative]::SetForegroundWindow($Hwnd) | Out-Null
	$redrawFlags = [uint32]([AgentServerMonitorNative]::RDW_INVALIDATE -bor [AgentServerMonitorNative]::RDW_ERASE -bor [AgentServerMonitorNative]::RDW_UPDATENOW)
	[AgentServerMonitorNative]::RedrawWindow($Hwnd, [IntPtr]::Zero, [IntPtr]::Zero, $redrawFlags) | Out-Null
	[AgentServerMonitorNative]::UpdateWindow($Hwnd) | Out-Null
	Start-Sleep -Milliseconds 100
}

function Save-ServerWindowCapture([IntPtr] $Hwnd, [string] $Path)
{
	$parent = Split-Path -Parent $Path
	if (-not [string]::IsNullOrWhiteSpace($parent))
	{
		[IO.Directory]::CreateDirectory($parent) | Out-Null
	}

	$clientRect = [AgentServerMonitorNative+RECT]::new()
	if (-not [AgentServerMonitorNative]::GetClientRect($Hwnd, [ref] $clientRect))
	{
		throw 'GetClientRect failed while preparing the server-monitor capture.'
	}
	$width = $clientRect.right - $clientRect.left
	$height = $clientRect.bottom - $clientRect.top
	if ($width -le 0 -or $height -le 0)
	{
		throw ('Server Monitor window client area is empty: {0}x{1}.' -f $width, $height)
	}

	$bitmap = $null
	$graphics = $null
	$hdc = [IntPtr]::Zero
	try
	{
		$bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
		$graphics = [Drawing.Graphics]::FromImage($bitmap)
		$graphics.Clear([Drawing.Color]::Black)
		$hdc = $graphics.GetHdc()
		try
		{
			if (-not [AgentServerMonitorNative]::PrintWindow($Hwnd, $hdc, ([AgentServerMonitorNative]::PW_CLIENTONLY -bor [AgentServerMonitorNative]::PW_RENDERFULLCONTENT)))
			{
				throw 'PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT) failed for the monitor window.'
			}
		}
		finally
		{
			if ($hdc -ne [IntPtr]::Zero)
			{
				$graphics.ReleaseHdc($hdc)
				$hdc = [IntPtr]::Zero
			}
		}
		$bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
	}
	finally
	{
		if ($graphics -ne $null)
		{
			$graphics.Dispose()
		}
		if ($bitmap -ne $null)
		{
			$bitmap.Dispose()
		}
	}

	$file = Get-Item -LiteralPath $Path -ErrorAction Stop
	if ($file.Length -le 0)
	{
		throw ('Server Monitor window capture is empty: {0}' -f $Path)
	}
	'Capture: {0} ({1} bytes, {2}x{3})' -f $Path, $file.Length, $width, $height
}

function Invoke-ServerClientClick([IntPtr] $Hwnd, [int] $ClientX, [int] $ClientY)
{
	$screenPoint = [AgentServerMonitorNative+POINT]::new()
	$screenPoint.X = $ClientX
	$screenPoint.Y = $ClientY
	if (-not [AgentServerMonitorNative]::ClientToScreen($Hwnd, [ref] $screenPoint))
	{
		throw ('ClientToScreen failed for ({0},{1}).' -f $ClientX, $ClientY)
	}
	if (-not [AgentServerMonitorNative]::SetCursorPos($screenPoint.X, $screenPoint.Y))
	{
		throw ('SetCursorPos failed for screen point ({0},{1}).' -f $screenPoint.X, $screenPoint.Y)
	}
	Start-Sleep -Milliseconds 50
	[AgentServerMonitorNative]::mouse_event([AgentServerMonitorNative]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
	[AgentServerMonitorNative]::mouse_event([AgentServerMonitorNative]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
}

function Restore-ServerWindowState([IntPtr] $Hwnd, $State)
{
	try
	{
		if ($State.Visible)
		{
			if ($State.Minimized)
			{
				[AgentServerMonitorNative]::ShowWindow($Hwnd, [AgentServerMonitorNative]::SW_MINIMIZE) | Out-Null
			}
			else
			{
				[AgentServerMonitorNative]::ShowWindow($Hwnd, [AgentServerMonitorNative]::SW_SHOWNOACTIVATE) | Out-Null
			}
		}
		else
		{
			[AgentServerMonitorNative]::ShowWindow($Hwnd, [AgentServerMonitorNative]::SW_HIDE) | Out-Null
		}
	}
	catch
	{
		Write-Warning ('Window visibility/minimized restoration failed: {0}' -f $_.Exception.Message)
	}

	try
	{
		$insertAfter = if ($State.Topmost) { [AgentServerMonitorNative]::HWND_TOPMOST } else { [AgentServerMonitorNative]::HWND_NOTOPMOST }
		$restoreFlags = [uint32]([AgentServerMonitorNative]::SWP_NOMOVE -bor [AgentServerMonitorNative]::SWP_NOSIZE -bor [AgentServerMonitorNative]::SWP_NOACTIVATE)
		[AgentServerMonitorNative]::SetWindowPos($Hwnd, $insertAfter, 0, 0, 0, 0, $restoreFlags) | Out-Null
		if ($State.Previous -ne [IntPtr]::Zero -and [AgentServerMonitorNative]::IsWindow($State.Previous))
		{
			[AgentServerMonitorNative]::SetWindowPos($Hwnd, $State.Previous, 0, 0, 0, 0, $restoreFlags) | Out-Null
		}
	}
	catch
	{
		Write-Warning ('Z-order restoration is best effort and failed: {0}' -f $_.Exception.Message)
	}

	try
	{
		[AgentServerMonitorNative]::SetCursorPos($State.CursorX, $State.CursorY) | Out-Null
	}
	catch
	{
		Write-Warning ('Cursor restoration failed: {0}' -f $_.Exception.Message)
	}

	try
	{
		if ($State.Foreground -ne [IntPtr]::Zero -and [AgentServerMonitorNative]::IsWindow($State.Foreground))
		{
			[AgentServerMonitorNative]::SetForegroundWindow($State.Foreground) | Out-Null
		}
	}
	catch
	{
		Write-Warning ('Foreground restoration is best effort and failed: {0}' -f $_.Exception.Message)
	}
}

$serverProcess = Get-Process -Id $MonitorPid -ErrorAction Stop
if ($serverProcess.HasExited)
{
	throw ('Retained monitor PID {0} has already exited.' -f $MonitorPid)
}
$serverWindowHwnd = [AgentServerMonitorNative]::FindTopLevelWindow($MonitorPid)
if ($serverWindowHwnd -eq [IntPtr]::Zero)
{
	throw ('No top-level HWND owned by retained monitor PID {0} was found.' -f $MonitorPid)
}

$beforeState = Get-ServerWindowState $serverWindowHwnd
Write-ServerWindowState 'Before' $beforeState
try
{
	Refresh-ServerWindow $serverWindowHwnd
	Save-ServerWindowCapture $serverWindowHwnd $BeforeCapturePath

	switch ($Target)
	{
		'Map'
		{
			# Client coordinates: Map tab center is (300,16).
			Invoke-ServerClientClick $serverWindowHwnd 300 16
			break
		}
		'Profile'
		{
			# Client coordinates: Profile tab center is (400,16).
			Invoke-ServerClientClick $serverWindowHwnd 400 16
			break
		}
		'ProfileCopy'
		{
			# Paint Profile before reading its live client height or locating Copy.
			Invoke-ServerClientClick $serverWindowHwnd 400 16
			Refresh-ServerWindow $serverWindowHwnd
			$profileClientRect = [AgentServerMonitorNative+RECT]::new()
			if (-not [AgentServerMonitorNative]::GetClientRect($serverWindowHwnd, [ref] $profileClientRect))
			{
				throw 'GetClientRect failed after Profile painted.'
			}
			$clientHeight = $profileClientRect.bottom - $profileClientRect.top
			$copyRect = [pscustomobject]@{
				Left = 260
				Right = 340
				Top = $clientHeight - 48
				Bottom = $clientHeight - 20
			}
			$copyCenterX = [int](($copyRect.Left + $copyRect.Right) / 2)
			$copyCenterY = [int](($copyRect.Top + $copyRect.Bottom) / 2)
			'Profile Copy client rectangle x={0}..{1}, y={2}..{3}; center=({4},{5})' -f `
				$copyRect.Left, $copyRect.Right, $copyRect.Top, $copyRect.Bottom, $copyCenterX, $copyCenterY
			Invoke-ServerClientClick $serverWindowHwnd $copyCenterX $copyCenterY
			break
		}
	}

	Refresh-ServerWindow $serverWindowHwnd
	Save-ServerWindowCapture $serverWindowHwnd $AfterCapturePath
}
finally
{
	Restore-ServerWindowState $serverWindowHwnd $beforeState
	if ([AgentServerMonitorNative]::IsWindow($serverWindowHwnd))
	{
		try
		{
			$restoredState = Get-ServerWindowState $serverWindowHwnd
			Write-ServerWindowState 'After restore' $restoredState
		}
		catch
		{
			Write-Warning ('Could not record post-restore state: {0}' -f $_.Exception.Message)
		}
	}
}
```

From the adopted worktree root, run the ignored copy with the retained PID (the `Profile` target is the acceptance scenario):

```powershell
pwsh -NoProfile -File .\Temp\Capture-ServerMonitor.ps1 -MonitorPid <exact retained monitor PID> -Target Profile
```

Run the acceptance scenario with `-Target Profile`; it switches only the monitor's Profile tab and writes the before/after captures. The optional `-Target ProfileCopy` click uses the live painted client height and the Copy rectangle `x=260..340`, `y=clientHeight-48..clientHeight-20`; it replaces the current clipboard contents, so use it only when clipboard mutation is intended. `Map` and `Profile` centers are `(300,16)` and `(400,16)` in client coordinates.

This recipe changes the interactive Windows desktop: it moves the physical
cursor, temporarily changes foreground focus and z-order, and (for
`ProfileCopy`) replaces clipboard contents. Visibility, minimized state, topmost
state, and cursor are restored in `finally`; foreground and exact z-order
restoration are best effort.
