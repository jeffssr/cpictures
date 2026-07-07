Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Xml

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourceSvg = Join-Path $ScriptDir "cpictures-file.svg"
$Sizes = @(16, 24, 32, 48, 64, 128, 256)

function Read-SvgPaths {
    param([string]$Path)
    [xml]$svg = Get-Content -LiteralPath $Path -Raw
    $namespace = New-Object System.Xml.XmlNamespaceManager($svg.NameTable)
    $namespace.AddNamespace("svg", "http://www.w3.org/2000/svg")
    $nodes = $svg.SelectNodes("//svg:path", $namespace)
    $paths = @()
    foreach ($node in $nodes) {
        $paths += [pscustomobject]@{
            Data = $node.d
            Fill = if ($node.fill) { $node.fill } else { "#000000" }
        }
    }
    $paths
}

function New-Tokenizer {
    param([string]$Data)
    $matches = [regex]::Matches($Data, "[A-Za-z]|[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?")
    $tokens = @()
    foreach ($match in $matches) {
        $tokens += $match.Value
    }
    $tokens
}

function Test-CommandToken {
    param([string]$Token)
    $Token -match "^[A-Za-z]$"
}

function Add-SvgArc {
    param(
        [System.Drawing.Drawing2D.GraphicsPath]$Path,
        [float]$StartX,
        [float]$StartY,
        [float]$Rx,
        [float]$Ry,
        [float]$XAxisRotation,
        [int]$LargeArcFlag,
        [int]$SweepFlag,
        [float]$EndX,
        [float]$EndY
    )

    if ($Rx -eq 0 -or $Ry -eq 0 -or ($StartX -eq $EndX -and $StartY -eq $EndY)) {
        $Path.AddLine($StartX, $StartY, $EndX, $EndY)
        return
    }

    $rx = [Math]::Abs([double]$Rx)
    $ry = [Math]::Abs([double]$Ry)
    $phi = ([double]$XAxisRotation) * [Math]::PI / 180.0
    $cosPhi = [Math]::Cos($phi)
    $sinPhi = [Math]::Sin($phi)
    $dx = ([double]$StartX - [double]$EndX) / 2.0
    $dy = ([double]$StartY - [double]$EndY) / 2.0
    $x1p = ($cosPhi * $dx) + ($sinPhi * $dy)
    $y1p = (-$sinPhi * $dx) + ($cosPhi * $dy)

    $lambda = (($x1p * $x1p) / ($rx * $rx)) + (($y1p * $y1p) / ($ry * $ry))
    if ($lambda -gt 1.0) {
        $scale = [Math]::Sqrt($lambda)
        $rx *= $scale
        $ry *= $scale
    }

    $rx2 = $rx * $rx
    $ry2 = $ry * $ry
    $x1p2 = $x1p * $x1p
    $y1p2 = $y1p * $y1p
    $denom = ($rx2 * $y1p2) + ($ry2 * $x1p2)
    if ($denom -eq 0) {
        $Path.AddLine($StartX, $StartY, $EndX, $EndY)
        return
    }

    $sign = if (($LargeArcFlag -ne 0) -ne ($SweepFlag -ne 0)) { 1.0 } else { -1.0 }
    $coef = $sign * [Math]::Sqrt([Math]::Max(0.0, (($rx2 * $ry2) - ($rx2 * $y1p2) - ($ry2 * $x1p2)) / $denom))
    $cxp = $coef * (($rx * $y1p) / $ry)
    $cyp = $coef * (-(($ry * $x1p) / $rx))
    $cx = ($cosPhi * $cxp) - ($sinPhi * $cyp) + (([double]$StartX + [double]$EndX) / 2.0)
    $cy = ($sinPhi * $cxp) + ($cosPhi * $cyp) + (([double]$StartY + [double]$EndY) / 2.0)

    $ux = ($x1p - $cxp) / $rx
    $uy = ($y1p - $cyp) / $ry
    $vx = (-$x1p - $cxp) / $rx
    $vy = (-$y1p - $cyp) / $ry
    $theta = [Math]::Atan2($uy, $ux)
    $delta = [Math]::Atan2(($ux * $vy) - ($uy * $vx), ($ux * $vx) + ($uy * $vy))
    if ($SweepFlag -eq 0 -and $delta -gt 0) {
        $delta -= 2.0 * [Math]::PI
    } elseif ($SweepFlag -ne 0 -and $delta -lt 0) {
        $delta += 2.0 * [Math]::PI
    }

    $segments = [Math]::Max(4, [int][Math]::Ceiling([Math]::Abs($delta) / ([Math]::PI / 18.0)))
    [double]$prevX = $StartX
    [double]$prevY = $StartY
    for ($step = 1; $step -le $segments; $step++) {
        $t = $theta + ($delta * ($step / [double]$segments))
        $px = $cx + ($cosPhi * $rx * [Math]::Cos($t)) - ($sinPhi * $ry * [Math]::Sin($t))
        $py = $cy + ($sinPhi * $rx * [Math]::Cos($t)) + ($cosPhi * $ry * [Math]::Sin($t))
        $Path.AddLine([float]$prevX, [float]$prevY, [float]$px, [float]$py)
        $prevX = $px
        $prevY = $py
    }
}

function Convert-SvgPath {
    param([string]$Data)
    $tokens = New-Tokenizer $Data
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new([System.Drawing.Drawing2D.FillMode]::Winding)
    $i = 0
    $cmd = ""
    [float]$x = 0
    [float]$y = 0
    [float]$startX = 0
    [float]$startY = 0
    [float]$lastCpx = 0
    [float]$lastCpy = 0
    $lastWasCubic = $false

    function NextNumber {
        if ($script:i -ge $script:tokens.Count) { throw "Unexpected end of SVG path." }
        [float]::Parse($script:tokens[$script:i++], [System.Globalization.CultureInfo]::InvariantCulture)
    }

    $script:tokens = $tokens
    $script:i = $i

    while ($script:i -lt $script:tokens.Count) {
        if (Test-CommandToken ($script:tokens[$script:i])) {
            $cmd = $script:tokens[$script:i++]
        }
        if ($cmd -eq "") { throw "SVG path command missing." }

        switch -CaseSensitive ($cmd) {
            "M" {
                $x = NextNumber
                $y = NextNumber
                $path.StartFigure()
                $startX = $x
                $startY = $y
                $cmd = "L"
                $lastWasCubic = $false
            }
            "m" {
                $x += NextNumber
                $y += NextNumber
                $path.StartFigure()
                $startX = $x
                $startY = $y
                $cmd = "l"
                $lastWasCubic = $false
            }
            "L" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $nx = NextNumber
                    $ny = NextNumber
                    $path.AddLine($x, $y, $nx, $ny)
                    $x = $nx
                    $y = $ny
                }
                $lastWasCubic = $false
            }
            "l" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $nx = $x + (NextNumber)
                    $ny = $y + (NextNumber)
                    $path.AddLine($x, $y, $nx, $ny)
                    $x = $nx
                    $y = $ny
                }
                $lastWasCubic = $false
            }
            "H" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $nx = NextNumber
                    $path.AddLine($x, $y, $nx, $y)
                    $x = $nx
                }
                $lastWasCubic = $false
            }
            "h" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $nx = $x + (NextNumber)
                    $path.AddLine($x, $y, $nx, $y)
                    $x = $nx
                }
                $lastWasCubic = $false
            }
            "V" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $ny = NextNumber
                    $path.AddLine($x, $y, $x, $ny)
                    $y = $ny
                }
                $lastWasCubic = $false
            }
            "v" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $ny = $y + (NextNumber)
                    $path.AddLine($x, $y, $x, $ny)
                    $y = $ny
                }
                $lastWasCubic = $false
            }
            "C" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $x1 = NextNumber; $y1 = NextNumber
                    $x2 = NextNumber; $y2 = NextNumber
                    $x3 = NextNumber; $y3 = NextNumber
                    $path.AddBezier($x, $y, $x1, $y1, $x2, $y2, $x3, $y3)
                    $x = $x3; $y = $y3; $lastCpx = $x2; $lastCpy = $y2
                    $lastWasCubic = $true
                }
            }
            "c" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $x1 = $x + (NextNumber); $y1 = $y + (NextNumber)
                    $x2 = $x + (NextNumber); $y2 = $y + (NextNumber)
                    $x3 = $x + (NextNumber); $y3 = $y + (NextNumber)
                    $path.AddBezier($x, $y, $x1, $y1, $x2, $y2, $x3, $y3)
                    $x = $x3; $y = $y3; $lastCpx = $x2; $lastCpy = $y2
                    $lastWasCubic = $true
                }
            }
            "S" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    if ($lastWasCubic) {
                        $x1 = (2 * $x) - $lastCpx
                        $y1 = (2 * $y) - $lastCpy
                    } else {
                        $x1 = $x
                        $y1 = $y
                    }
                    $x2 = NextNumber; $y2 = NextNumber
                    $x3 = NextNumber; $y3 = NextNumber
                    $path.AddBezier($x, $y, $x1, $y1, $x2, $y2, $x3, $y3)
                    $x = $x3; $y = $y3; $lastCpx = $x2; $lastCpy = $y2
                    $lastWasCubic = $true
                }
            }
            "s" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    if ($lastWasCubic) {
                        $x1 = (2 * $x) - $lastCpx
                        $y1 = (2 * $y) - $lastCpy
                    } else {
                        $x1 = $x
                        $y1 = $y
                    }
                    $x2 = $x + (NextNumber); $y2 = $y + (NextNumber)
                    $x3 = $x + (NextNumber); $y3 = $y + (NextNumber)
                    $path.AddBezier($x, $y, $x1, $y1, $x2, $y2, $x3, $y3)
                    $x = $x3; $y = $y3; $lastCpx = $x2; $lastCpy = $y2
                    $lastWasCubic = $true
                }
            }
            "A" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $rx = NextNumber; $ry = NextNumber; $angle = NextNumber
                    $large = [int](NextNumber); $sweep = [int](NextNumber)
                    $nx = NextNumber; $ny = NextNumber
                    Add-SvgArc $path $x $y $rx $ry $angle $large $sweep $nx $ny
                    $x = $nx; $y = $ny
                }
                $lastWasCubic = $false
            }
            "a" {
                while ($script:i -lt $script:tokens.Count -and !(Test-CommandToken ($script:tokens[$script:i]))) {
                    $rx = NextNumber; $ry = NextNumber; $angle = NextNumber
                    $large = [int](NextNumber); $sweep = [int](NextNumber)
                    $nx = $x + (NextNumber); $ny = $y + (NextNumber)
                    Add-SvgArc $path $x $y $rx $ry $angle $large $sweep $nx $ny
                    $x = $nx; $y = $ny
                }
                $lastWasCubic = $false
            }
            "Z" {
                $path.CloseFigure()
                $x = $startX
                $y = $startY
                $lastWasCubic = $false
            }
            "z" {
                $path.CloseFigure()
                $x = $startX
                $y = $startY
                $lastWasCubic = $false
            }
            default {
                throw "Unsupported SVG path command: $cmd"
            }
        }
    }

    $path
}

function Save-IconPng {
    param(
        [int]$Size,
        [object[]]$Paths
    )
    $bitmap = [System.Drawing.Bitmap]::new($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.ScaleTransform($Size / 1024.0, $Size / 1024.0)

    foreach ($entry in $Paths) {
        $brush = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml($entry.Fill))
        $graphics.FillPath($brush, $entry.Path)
        $brush.Dispose()
    }

    $out = Join-Path $ScriptDir "cpictures-file-$Size.png"
    $bitmap.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    if ($Size -eq 256) {
        $bitmap.Save((Join-Path $ScriptDir "cpictures-file.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $graphics.Dispose()
    $bitmap.Dispose()
    $out
}

function Write-UInt16 {
    param([System.IO.BinaryWriter]$Writer, [int]$Value)
    $Writer.Write([uint16]$Value)
}

function Write-UInt32 {
    param([System.IO.BinaryWriter]$Writer, [long]$Value)
    $Writer.Write([uint32]$Value)
}

function Write-Ico {
    param([string[]]$PngPaths, [string]$OutputPath)
    $entries = @()
    foreach ($path in $PngPaths) {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $image = [System.Drawing.Image]::FromFile($path)
        $entries += [pscustomobject]@{ Width = $image.Width; Height = $image.Height; Bytes = $bytes }
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

$sourcePaths = Read-SvgPaths $SourceSvg
$paths = @()
foreach ($entry in $sourcePaths) {
    $paths += [pscustomobject]@{
        Path = Convert-SvgPath $entry.Data
        Fill = $entry.Fill
    }
}

$pngPaths = @()
foreach ($size in $Sizes) {
    $pngPaths += Save-IconPng $size $paths
}

foreach ($entry in $paths) {
    $entry.Path.Dispose()
}

Write-Ico $pngPaths (Join-Path $ScriptDir "cpictures-file.ico")
Write-Output (Join-Path $ScriptDir "cpictures-file.ico")
