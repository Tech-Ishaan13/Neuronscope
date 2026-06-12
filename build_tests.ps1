# Build Script for NeuronScope Tests (Pure C++)
Write-Host "Compiling test suite..." -ForegroundColor Cyan

g++ -std=c++17 -Isrc -Iinclude -o neuronscope_tests.exe tests/test_all.cpp

if ($LASTEXITCODE -eq 0) {
    Write-Host "Tests built successfully! Run .\neuronscope_tests.exe to verify." -ForegroundColor Green
} else {
    Write-Error "Test compilation failed with exit code $LASTEXITCODE."
}
