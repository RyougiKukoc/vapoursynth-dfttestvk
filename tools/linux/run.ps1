param(
    [string]$Proxy = 'http://host.docker.internal:7890',
    [string]$Output = '',
    [switch]$Runtime
)
$ErrorActionPreference = 'Stop'
$dftRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if (-not $Output) { $Output = Join-Path $dftRoot ('results/linux-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$dftOutput = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $dftOutput) { throw 'Select a new output directory to preserve earlier evidence' }
New-Item -ItemType Directory -Path $dftOutput | Out-Null
$dftStage = if ($Runtime) { 'runtime' } else { 'validation' }
$dftImage = 'dfttestvk-linux-' + $dftStage + ':py313'
& docker build --target $dftStage -f (Join-Path $PSScriptRoot 'Dockerfile') `
    --build-arg "HTTP_PROXY=$Proxy" --build-arg "HTTPS_PROXY=$Proxy" `
    --build-arg 'NO_PROXY=localhost,127.0.0.1' -t $dftImage $PSScriptRoot
if ($LASTEXITCODE -ne 0) { throw 'Docker image build failed' }
$dftRunArgs = @('run', '--rm', '--mount', "type=bind,source=$dftRoot,target=/src,readonly", `
    '--mount', "type=bind,source=$dftOutput,target=/out", `
    '-e', "http_proxy=$Proxy", '-e', "https_proxy=$Proxy", '-e', 'LP_NUM_THREADS=4', $dftImage)
& docker @dftRunArgs python /src/tools/linux/verify.py
if ($LASTEXITCODE -ne 0) { throw 'Linux compiler/ELF/FFT checks failed' }
if ($Runtime) {
    & docker @dftRunArgs python /src/tools/linux/runtime_info.py
    if ($LASTEXITCODE -ne 0) { throw 'VapourSynth loading/SDK check failed' }
    & docker @dftRunArgs python /src/tests/correctness.py --backend vk --dll /out/build-gcc/dfttestvk.so `
        --suite full --threads 4 --output /out/gcc-runtime-full.json
    if ($LASTEXITCODE -ne 0) { throw 'Linux Vulkan correctness checks failed' }
    & docker @dftRunArgs python /src/tests/cache.py --backend vk --dll /out/build-gcc/dfttestvk.so `
        --frames 7 --threads 8 --rounds 1 --output /out/gcc-runtime-cache.json
    if ($LASTEXITCODE -ne 0) { throw 'Linux Vulkan cache checks failed' }
    & docker @dftRunArgs python /src/tests/correctness.py --backend vk --dll /out/build-clang/dfttestvk.so `
        --suite smoke --threads 4 --output /out/clang-runtime-smoke.json
    if ($LASTEXITCODE -ne 0) { throw 'Clang runtime smoke checks failed' }
}
Write-Output "Linux checks completed: $dftOutput"
