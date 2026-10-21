"""Serial A/B of cached DFT frequency-row lifetime, with identical output graphs."""
import argparse
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys

ap = argparse.ArgumentParser()
ap.add_argument('--results', type=Path, required=True)
ap.add_argument('--frames', type=int, default=512)
ap.add_argument('--temporal', default='3,7')
args = ap.parse_args()
root = Path('D:/WorkSpace-bm3d/vapoursynth-dfttestvk')
skill = root.parent / 'holywu-vulkan-skill'
args.results.mkdir(parents=True, exist_ok=False)
cases = [(v, fmt, t) for fmt in ('gray16', 'yuv420p16') for t in map(int, args.temporal.split(',')) for v in ('baseline', 'row-frequency')]
rows = []
for repeat in range(3):
    for variant, fmt, temporal in cases[repeat:] + cases[:repeat]:
        name = f'{variant}-{fmt}-t{temporal}'
        prefix = args.results / f'{name}-r{repeat}'
        config = {'backend': 'vk', 'format': fmt, 'dll': str(root / f'build/r2-study/{variant}.dll'),
                  'raw_yuv420p16': str(root.parent / 'sample-1080p.yuv'),
                  'params': {'sbsize': 16, 'sosize': 12, 'tbsize': temporal, 'twin': 7}}
        prefix.with_suffix('.config.json').write_text(json.dumps(config, indent=2), encoding='utf-8')
        command = [sys.executable, str(Path(__file__).with_name('run_isolated.py')),
                   str(skill / 'scripts/benchmark_vs.py'), '--factory', str(root / 'tests/bench_factory.py') + ':make',
                   '--config', str(prefix.with_suffix('.config.json')), '--output', str(prefix.with_suffix('.json')),
                   '--frames', str(args.frames), '--warmup', '16', '--threads', '16', '--prefetch', '8']
        with prefix.with_suffix('.log').open('w', encoding='utf-8') as log, prefix.with_suffix('.telemetry.csv').open('w') as telem:
            telemetry = subprocess.Popen(['nvidia-smi', '--query-gpu=timestamp,utilization.gpu,memory.used,temperature.gpu,power.draw,clocks.sm',
                                          '--format=csv', '--loop-ms=500'], stdout=telem, stderr=subprocess.STDOUT)
            try:
                subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT, check=True)
            finally:
                telemetry.terminate()
                telemetry.wait(timeout=15)
        data = json.loads(prefix.with_suffix('.json').read_text(encoding='utf-8'))
        rows.append({'name': name, 'repeat': repeat, 'fps': data['fps']})
        print(name, repeat, data['fps'], flush=True)
summary = {}
for v, fmt, t in cases:
    name = f'{v}-{fmt}-t{t}'
    rates = [r['fps'] for r in rows if r['name'] == name]
    summary[name] = {'fps': rates, 'median': statistics.median(rates), 'min': min(rates), 'max': max(rates)}
(args.results / 'summary.json').write_text(json.dumps({'rows': rows, 'summary': summary}, indent=2), encoding='utf-8')
print(json.dumps(summary, indent=2))
