# MSVC Build Script for NeuronScope (Pure C++)
Write-Host "Locating FTXUI and Application C++ source files..." -ForegroundColor Cyan

# Find all FTXUI C++ files excluding tests, fuzzers, and benchmarks
$ftxui_files = Get-ChildItem -Path FTXUI/src/ftxui -Filter *.cpp -Recurse | 
               Where-Object { $_.Name -notmatch "test|fuzzer|benchmark" -and $_.Name -ne "loop.cpp" } | 
               Foreach-Object { $_.FullName }

# Find all application C++ files
$app_files = Get-ChildItem -Path src -Filter *.cpp -Recurse | 
             Foreach-Object { $_.FullName }

New-Item -ItemType Directory -Force -Path obj | Out-Null
$all_files = $ftxui_files + $app_files

$max_jobs = [int]$env:NUMBER_OF_PROCESSORS
if ($max_jobs -lt 1) { $max_jobs = 4 }
Write-Host "Using max concurrency: $max_jobs" -ForegroundColor Cyan

$jobs = @()
$failed_jobs = 0

foreach ($file in $all_files) {
    # Limit concurrency
    while (($jobs | Where-Object { -not $_.HasExited }).Count -ge $max_jobs) {
        Start-Sleep -Milliseconds 20
    }

    $obj_name = $file.Replace(":\", "_").Replace("\", "_").Replace("/", "_").Replace(":", "_") + ".obj"
    $obj_path = "obj\$obj_name"

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cl.exe"
    $psi.Arguments = "/nologo /EHsc /std:c++17 /O2 /MT /I`"src`" /I`"include`" /I`"FTXUI\include`" /I`"FTXUI\src`" /c `"$file`" /Fo`"$obj_path`""
    $psi.UseShellExecute = $false

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    
    # Custom property to keep track of filename
    $p = Add-Member -InputObject $p -MemberType NoteProperty -Name "FileName" -Value $(Split-Path $file -Leaf) -PassThru

    Write-Host "Compiling: $($p.FileName)..." -ForegroundColor Yellow
    [void]$p.Start()
    $jobs += $p
}

# Wait for all remaining jobs
Write-Host "Waiting for all compile jobs to finish..." -ForegroundColor Cyan
while (($jobs | Where-Object { -not $_.HasExited }).Count -gt 0) {
    Start-Sleep -Milliseconds 50
}

# Check exit codes
$obj_files = @()
foreach ($job in $jobs) {
    if ($job.ExitCode -ne 0) {
        Write-Host "Error compiling $($job.FileName)" -ForegroundColor Red
        $failed_jobs++
    }
}

if ($failed_jobs -gt 0) {
    Write-Error "Compilation failed for $failed_jobs files."
    exit 1
}

# Re-generate obj list for link step
foreach ($file in $all_files) {
    $obj_name = $file.Replace(":\", "_").Replace("\", "_").Replace("/", "_").Replace(":", "_") + ".obj"
    $obj_files += "obj\$obj_name"
}

Write-Host "Linking neuronscope.exe..." -ForegroundColor Cyan
& cl.exe /nologo /MT /Feneuronscope.exe $obj_files

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Succeeded! Run .\neuronscope.exe to start the app." -ForegroundColor Green
} else {
    Write-Error "Build Failed with exit code $LASTEXITCODE."
}
