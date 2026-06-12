# Build Script for NeuronScope (Pure C++)
Write-Host "Locating FTXUI and Application C++ source files..." -ForegroundColor Cyan

# Find all FTXUI C++ files excluding tests, fuzzers, and benchmarks
$ftxui_files = Get-ChildItem -Path FTXUI/src/ftxui -Filter *.cpp -Recurse | 
               Where-Object { $_.Name -notmatch "test|fuzzer|benchmark" } | 
               Foreach-Object { $_.FullName }

# Find all application C++ files
$app_files = Get-ChildItem -Path src -Filter *.cpp -Recurse | 
             Foreach-Object { $_.FullName }

Write-Host "Compiling $($ftxui_files.Count) FTXUI files and $($app_files.Count) App files..." -ForegroundColor Cyan

# Compile executable
g++ -std=c++17 -O3 -Isrc -Iinclude -IFTXUI/include -o neuronscope.exe $ftxui_files $app_files

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build Succeeded! Run .\neuronscope.exe to start the app." -ForegroundColor Green
} else {
    Write-Error "Build Failed with exit code $LASTEXITCODE."
}
