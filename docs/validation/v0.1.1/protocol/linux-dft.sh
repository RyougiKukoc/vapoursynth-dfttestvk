set -eu
python /src/tools/linux/verify.py --source /src --output /out
python /src/tests/run_isolated.py /src/tests/correctness.py --backend vk --dll /out/build-gcc/dfttestvk.so --suite full --threads 4 --output /out/full.json
python /src/tests/run_isolated.py /src/tests/cache.py --dll /out/build-gcc/dfttestvk.so --output /out/cache.json
python /src/tests/run_isolated.py /src/tests/correctness.py --backend vk --dll /out/build-clang/dfttestvk.so --suite smoke --threads 4 --output /out/clang-smoke.json
