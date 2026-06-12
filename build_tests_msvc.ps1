# MSVC Build Script for NeuronScope Tests (Pure C++)
Write-Host "Compiling test suite with MSVC cl.exe..." -ForegroundColor Cyan

cl.exe /EHsc /std:c++17 /Isrc /Iinclude /Feneuronscope_tests.exe tests/test_all.cpp

if ($LASTEXITCODE -eq 0) {
    Write-Host "Tests compiled successfully! Run .\neuronscope_tests.exe to verify." -ForegroundColor Green
} else {
    Write-Error "Test compilation failed with exit code $LASTEXITCODE."
}
