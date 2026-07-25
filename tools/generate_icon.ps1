param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\assets")
)

Add-Type -AssemblyName System.Drawing

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

function New-SpectraIconFrame {
    param([int]$Size)

    $scale = $Size / 256.0
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $tilePath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $tilePath.AddArc(8 * $scale, 8 * $scale, 104 * $scale, 104 * $scale, 180, 90)
    $tilePath.AddArc(144 * $scale, 8 * $scale, 104 * $scale, 104 * $scale, 270, 90)
    $tilePath.AddArc(144 * $scale, 144 * $scale, 104 * $scale, 104 * $scale, 0, 90)
    $tilePath.AddArc(8 * $scale, 144 * $scale, 104 * $scale, 104 * $scale, 90, 90)
    $tilePath.CloseFigure()
    $tileBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 250, 250, 250))
    $graphics.FillPath($tileBrush, $tilePath)

    $wavePath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $wavePath.StartFigure()
    $wavePath.AddBezier(28 * $scale, 128 * $scale, 45 * $scale, 128 * $scale, 47 * $scale, 72 * $scale, 64 * $scale, 72 * $scale)
    $wavePath.AddBezier(64 * $scale, 72 * $scale, 81 * $scale, 72 * $scale, 83 * $scale, 184 * $scale, 100 * $scale, 184 * $scale)
    $wavePath.AddBezier(100 * $scale, 184 * $scale, 117 * $scale, 184 * $scale, 119 * $scale, 72 * $scale, 136 * $scale, 72 * $scale)
    $wavePath.AddBezier(136 * $scale, 72 * $scale, 153 * $scale, 72 * $scale, 155 * $scale, 184 * $scale, 172 * $scale, 184 * $scale)
    $wavePath.AddBezier(172 * $scale, 184 * $scale, 189 * $scale, 184 * $scale, 191 * $scale, 128 * $scale, 228 * $scale, 128 * $scale)
    $wavePen = New-Object System.Drawing.Pen([System.Drawing.Color]::Black, [Math]::Max(1.25, 18 * $scale))
    $wavePen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $wavePen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $wavePen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawPath($wavePen, $wavePath)

    $memory = New-Object System.IO.MemoryStream
    $bitmap.Save($memory, [System.Drawing.Imaging.ImageFormat]::Png)
    [byte[]]$bytes = $memory.ToArray()

    $memory.Dispose()
    $wavePen.Dispose()
    $wavePath.Dispose()
    $tileBrush.Dispose()
    $tilePath.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()

    Write-Output -NoEnumerate $bytes
}

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$frames = @()
foreach ($size in $sizes) {
    $frames += ,([byte[]](New-SpectraIconFrame -Size $size))
}

$pngPath = Join-Path $resolvedOutput "spectra-icon.png"
$icoPath = Join-Path $resolvedOutput "spectra.ico"
[System.IO.File]::WriteAllBytes($pngPath, $frames[$frames.Count - 1])

$stream = [System.IO.File]::Open($icoPath, [System.IO.FileMode]::Create)
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$frames.Count)

$offset = 6 + 16 * $frames.Count
for ($index = 0; $index -lt $frames.Count; $index++) {
    $size = $sizes[$index]
    $bytes = $frames[$index]
    $writer.Write([Byte]$(if ($size -eq 256) { 0 } else { $size }))
    $writer.Write([Byte]$(if ($size -eq 256) { 0 } else { $size }))
    $writer.Write([Byte]0)
    $writer.Write([Byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $bytes.Length
}

foreach ($bytes in $frames) {
    $writer.Write($bytes)
}
$writer.Dispose()

Write-Output "Generated $pngPath"
Write-Output "Generated $icoPath"
