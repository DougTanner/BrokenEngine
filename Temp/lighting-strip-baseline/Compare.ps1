param(
	[Parameter(Mandatory)][string]$A,
	[Parameter(Mandatory)][string]$B,
	[int]$Threshold = 8
)
Add-Type -AssemblyName System.Drawing
$ImageA = [System.Drawing.Bitmap]::new($A)
$ImageB = [System.Drawing.Bitmap]::new($B)
if ($ImageA.Width -ne $ImageB.Width -or $ImageA.Height -ne $ImageB.Height) { throw 'Size mismatch.' }
$Rect = [System.Drawing.Rectangle]::new(0, 0, $ImageA.Width, $ImageA.Height)
$Format = [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
$DataA = $ImageA.LockBits($Rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $Format)
$DataB = $ImageB.LockBits($Rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, $Format)
$Count = $DataA.Stride * $ImageA.Height
$BytesA = [byte[]]::new($Count)
$BytesB = [byte[]]::new($Count)
[System.Runtime.InteropServices.Marshal]::Copy($DataA.Scan0, $BytesA, 0, $Count)
[System.Runtime.InteropServices.Marshal]::Copy($DataB.Scan0, $BytesB, 0, $Count)
$ImageA.UnlockBits($DataA); $ImageB.UnlockBits($DataB)
$ImageA.Dispose(); $ImageB.Dispose()
$Different = 0; $OverThreshold = 0; $MaxDelta = 0; [int64]$SumDelta = 0
for ($i = 0; $i -lt $Count; $i += 4) {
	$Delta = [math]::Max([math]::Max([math]::Abs($BytesA[$i] - $BytesB[$i]), [math]::Abs($BytesA[$i + 1] - $BytesB[$i + 1])), [math]::Abs($BytesA[$i + 2] - $BytesB[$i + 2]))
	if ($Delta -gt 0) { $Different++; $SumDelta += $Delta }
	if ($Delta -gt $Threshold) { $OverThreshold++ }
	if ($Delta -gt $MaxDelta) { $MaxDelta = $Delta }
}
$Pixels = $Count / 4
"pixels=$Pixels differing=$Different ($([math]::Round(100.0 * $Different / $Pixels, 3))%) over$Threshold=$OverThreshold ($([math]::Round(100.0 * $OverThreshold / $Pixels, 3))%) maxDelta=$MaxDelta meanDeltaOfDiffering=$(if ($Different) { [math]::Round($SumDelta / $Different, 2) } else { 0 })"
