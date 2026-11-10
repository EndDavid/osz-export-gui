Add-Type -AssemblyName System.Drawing

$root = "E:\EUItest\osz-export-gui\assets"
$size = 1024
$base = [System.Drawing.Image]::FromFile("$root\source\lazer-256.png")

$bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.Clear([System.Drawing.Color]::Transparent)
$g.DrawImage($base, 0, 0, $size, $size)
$base.Dispose()

$shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(50, 0, 0, 0))
$g.FillEllipse($shadow, 762 - 254, 774 - 254, 508, 508)
$shadow.Dispose()

$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)
$g.FillEllipse($white, 762 - 250, 762 - 250, 500, 500)
$white.Dispose()

$pink = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 255, 102, 170)), 16
$g.DrawEllipse($pink, 762 - 242, 762 - 242, 484, 484)
$pink.Dispose()

$dark = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 28, 32, 40))
# tray (U shape)
$g.FillRectangle($dark, 612, 772, 32, 128)
$g.FillRectangle($dark, 880, 772, 32, 128)
$g.FillRectangle($dark, 612, 868, 300, 32)
# arrow
$g.FillRectangle($dark, 717, 700, 60, 168)
$points = @(
    (New-Object System.Drawing.Point 747, 574),
    (New-Object System.Drawing.Point 662, 712),
    (New-Object System.Drawing.Point 832, 712)
)
$g.FillPolygon($dark, $points)
$dark.Dispose()

$g.Dispose()
$bmp.Save("$root\icon-1024.png", [System.Drawing.Imaging.ImageFormat]::Png)

# Downscale to icon sizes.
$sizes = @(256, 128, 64, 48, 32, 24, 16)
foreach ($s in $sizes) {
    $dst = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $dg = [System.Drawing.Graphics]::FromImage($dst)
    $dg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $dg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $dg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $dg.Clear([System.Drawing.Color]::Transparent)
    $dg.DrawImage($bmp, 0, 0, $s, $s)
    $dg.Dispose()
    $dst.Save("$root\icon-$s.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}
$bmp.Dispose()
Copy-Item "$root\icon-256.png" "$root\icon.png" -Force
"done"
