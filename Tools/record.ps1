# 게임을 -game 창으로 띄워 시작 명령을 실행하고, 창 영역을 ffmpeg로 녹화한다.
# 사용: .\Tools\record.ps1 -Name solo -Cmds "CRToggleView, CRSpawn 30, CRDemo 5" -Seconds 30
#       -Mode Title | HitCheck(기본) | MapEdit | InGame 으로 시작 모드 선택
# 직접 띄운 게임 프로세스만 닫으므로 열려 있는 에디터는 건드리지 않는다.
# 결과: Recordings\<Name>.mp4 (+ 확인용 프레임 PNG). 튜닝 저장 파일은 녹화 뒤 지워서 사용자 설정을 건드리지 않는다.
param([string]$Name, [string]$Cmds, [int]$Seconds = 26, [string]$Mode = 'HitCheck')

$Project = Split-Path $PSScriptRoot -Parent
$OutDir = Join-Path $Project 'Recordings'
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$SaveFile = Join-Path $Project 'Saved\SaveGames\CRTuning.sav'
$HadSave = Test-Path $SaveFile
if ($HadSave) { Copy-Item $SaveFile "$SaveFile.bak" -Force }

Add-Type @"
using System; using System.Runtime.InteropServices;
public class CRRecordWin {
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
 [StructLayout(LayoutKind.Sequential)] public struct R { public int L,T,Rt,B; }
 [StructLayout(LayoutKind.Sequential)] public struct P { public int X,Y; }
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out R r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref P p);
}
"@
[CRRecordWin]::SetProcessDPIAware() | Out-Null

$GameArgs = @("`"$Project\CrashRumble.uproject`"", "/Game/Maps/L_Grid?game=$Mode", '-game', '-windowed', '-ResX=1280', '-ResY=720', '-WinX=100', '-WinY=100')
if ($Cmds) { $GameArgs += "-ExecCmds=`"$Cmds`"" }
$Game = Start-Process $Editor -ArgumentList $GameArgs -PassThru

# 로딩 스플래시가 아니라 게임 창이 뜰 때까지 기다린다
$Window = [IntPtr]::Zero
for ($i = 0; $i -lt 120 -and $Window -eq [IntPtr]::Zero; $i++) {
  Start-Sleep -Milliseconds 500
  $Game.Refresh()
  if ($Game.MainWindowTitle -like 'CrashRumble*') { $Window = $Game.MainWindowHandle }
}
Start-Sleep 2
[CRRecordWin]::SetWindowPos($Window, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null # 항상 위 (다른 창에 가려지지 않게)
[CRRecordWin]::SetForegroundWindow($Window) | Out-Null
$Rect = New-Object CRRecordWin+R; [CRRecordWin]::GetClientRect($Window, [ref]$Rect) | Out-Null
$Origin = New-Object CRRecordWin+P; [CRRecordWin]::ClientToScreen($Window, [ref]$Origin) | Out-Null
$Width = $Rect.Rt - ($Rect.Rt % 2); $Height = $Rect.B - ($Rect.B % 2)

$Video = Join-Path $OutDir "$Name.mp4"
ffmpeg -y -loglevel error -f gdigrab -framerate 30 -offset_x $Origin.X -offset_y $Origin.Y -video_size "${Width}x${Height}" -t $Seconds -i desktop `
  -c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p $Video

$Game.CloseMainWindow() | Out-Null
if (-not $Game.WaitForExit(5000)) { Stop-Process -Id $Game.Id -Force }

# 녹화용 cr.Tune 값이 사용자 설정으로 남지 않게 되돌린다
Remove-Item $SaveFile -ErrorAction SilentlyContinue
if ($HadSave) { Move-Item "$SaveFile.bak" $SaveFile -Force }

ffmpeg -y -loglevel error -i $Video -vf "fps=1,scale=320:-1,tile=6x5" -frames:v 1 (Join-Path $OutDir "${Name}_sheet.png")
Write-Output $Video
