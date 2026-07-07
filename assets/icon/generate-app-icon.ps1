Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Sizes = @(16, 24, 32, 48, 64, 128, 256)

function New-Color {
    param(
        [int]$A,
        [string]$Hex
    )
    $base = [System.Drawing.ColorTranslator]::FromHtml($Hex)
    [System.Drawing.Color]::FromArgb($A, $base.R, $base.G, $base.B)
}

function New-RoundedRectPath {
    param(
        [float]$X,
        [float]$Y,
        [float]$W,
        [float]$H,
        [float]$R
    )
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $d = $R * 2
    $path.AddArc($X, $Y, $d, $d, 180, 90)
    $path.AddArc($X + $W - $d, $Y, $d, $d, 270, 90)
    $path.AddArc($X + $W - $d, $Y + $H - $d, $d, $d, 0, 90)
    $path.AddArc($X, $Y + $H - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $path
}

function Draw-AppIcon {
    param(
        [System.Drawing.Graphics]$G,
        [int]$Size
    )
    $G.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $G.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $G.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $G.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $G.ScaleTransform($Size / 256.0, $Size / 256.0)

    $outer = New-RoundedRectPath 16 16 224 224 52
    $bgBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        [System.Drawing.RectangleF]::new(16, 16, 224, 224),
        (New-Color 255 "#070B16"),
        (New-Color 255 "#151A35"),
        45
    )
    $G.FillPath($bgBrush, $outer)
    $bgBrush.Dispose()

    $edgePen = [System.Drawing.Pen]::new((New-Color 95 "#5B6CFF"), 1.4)
    $G.DrawPath($edgePen, $outer)
    $edgePen.Dispose()

    $inner = New-RoundedRectPath 24 24 208 208 45
    $innerPen = [System.Drawing.Pen]::new((New-Color 35 "#FFFFFF"), 1)
    $G.DrawPath($innerPen, $inner)
    $innerPen.Dispose()
    $inner.Dispose()
    $outer.Dispose()

    $rect = [System.Drawing.RectangleF]::new(70, 66, 120, 126)
    $cBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
        [System.Drawing.RectangleF]::new(58, 54, 140, 150),
        (New-Color 255 "#43E8FF"),
        (New-Color 255 "#A77CFF"),
        35
    )
    $pen = [System.Drawing.Pen]::new($cBrush, 30)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $G.DrawArc($pen, $rect, 43, 274)
    $pen.Dispose()
    $cBrush.Dispose()
}

function Save-Png {
    param([int]$Size)
    $bitmap = [System.Drawing.Bitmap]::new($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    Draw-AppIcon $graphics $Size
    $graphics.Dispose()

    $path = Join-Path $ScriptDir "cpictures-$Size.png"
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    if ($Size -eq 256) {
        $bitmap.Save((Join-Path $ScriptDir "cpictures.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    if ($Size -eq 32) {
        $bitmap.Save((Join-Path $ScriptDir "cpictures-exe-preview.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $bitmap.Dispose()
    $path
}

function Write-UInt16 {
    param(
        [System.IO.BinaryWriter]$Writer,
        [int]$Value
    )
    $Writer.Write([uint16]$Value)
}

function Write-UInt32 {
    param(
        [System.IO.BinaryWriter]$Writer,
        [long]$Value
    )
    $Writer.Write([uint32]$Value)
}

function Write-Ico {
    param(
        [string[]]$PngPaths,
        [string]$OutputPath
    )
    $entries = @()
    foreach ($path in $PngPaths) {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $image = [System.Drawing.Image]::FromFile($path)
        $entries += [pscustomobject]@{
            Width = $image.Width
            Height = $image.Height
            Bytes = $bytes
        }
        $image.Dispose()
    }

    $stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    Write-UInt16 $writer 0
    Write-UInt16 $writer 1
    Write-UInt16 $writer $entries.Count

    $offset = 6 + (16 * $entries.Count)
    foreach ($entry in $entries) {
        $writer.Write([byte]$(if ($entry.Width -eq 256) { 0 } else { $entry.Width }))
        $writer.Write([byte]$(if ($entry.Height -eq 256) { 0 } else { $entry.Height }))
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        Write-UInt16 $writer 1
        Write-UInt16 $writer 32
        Write-UInt32 $writer $entry.Bytes.Length
        Write-UInt32 $writer $offset
        $offset += $entry.Bytes.Length
    }

    foreach ($entry in $entries) {
        $writer.Write($entry.Bytes)
    }

    $writer.Dispose()
    $stream.Dispose()
}

$pngPaths = @()
foreach ($size in $Sizes) {
    $pngPaths += Save-Png $size
}
Write-Ico $pngPaths (Join-Path $ScriptDir "cpictures.ico")
Write-Output (Join-Path $ScriptDir "cpictures.ico")
