# Copyright 2024 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Collects benchmarks for supported allocators."""

import argparse
import json
import os
import subprocess
import sys

from pathlib import Path
from typing import Any, IO


def _parse_args() -> argparse.Namespace:
    """Parse arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '-o', '--output', type=str, help='Path to write a CSV file to'
    )
    parser.add_argument(
        '-v', '--verbose', action='store_true', help='Echo benchmarks to stdout'
    )
    return parser.parse_args()


BAZEL = 'bazelisk'

ALLOCATORS = [
    'best_fit',
    'dual_first_fit',
    'first_fit',
    'last_fit',
    'worst_fit',
    'tlsf',
]

BY_ALLOC_COUNT = 'by allocation count'
BY_ALLOC_COUNT_1K = 'allocation count in [1,000, 10,000)'
BY_ALLOC_COUNT_BUCKETS = [
    'allocation count in [0, 10)',
    'allocation count in [10, 100)',
    'allocation count in [100, 1,000)',
    BY_ALLOC_COUNT_1K,
    'allocation count in [10,000, inf)',
]

BY_FRAGMENTATION = 'by fragmentation'
BY_FRAGMENTATION_BUCKETS = [
    'fragmentation in [0.0, 0.2)',
    'fragmentation in [0.2, 0.4)',
    'fragmentation in [0.4, 0.6)',
    'fragmentation in [0.6, 0.8)',
    'fragmentation in [0.8, 1.0]',
]

BY_ALLOC_SIZE = 'by allocation size'
BY_ALLOC_SIZE_BUCKETS = [
    'usable size in [0, 16)',
    'usable size in [16, 64)',
    'usable size in [64, 256)',
    'usable size in [256, 1024)',
    'usable size in [1024, 4096)',
    'usable size in [4096, inf)',
]

BUCKETS = {
    BY_ALLOC_COUNT: BY_ALLOC_COUNT_BUCKETS,
    BY_FRAGMENTATION: BY_FRAGMENTATION_BUCKETS,
    BY_ALLOC_SIZE: BY_ALLOC_SIZE_BUCKETS,
}

METRICS = [
    'mean response time (ns)',
    'mean fragmentation metric',
    'mean max available (bytes)',
    'number of calls that failed',
]


def find_pw_root() -> Path:
    """Returns the path to the Pigweed repository."""
    pw_root = os.getenv('PW_ROOT')
    if pw_root:
        return Path(pw_root)
    cwd = Path(os.getcwd())
    marker = 'PIGWEED_MODULES'
    while True:
        f = cwd / marker
        if f.is_file():
            return cwd
        if cwd.parent == cwd:
            raise RuntimeError('Unable to find Pigweed root')
        cwd = cwd.parent


class Benchmark:
    """Collection of benchmarks for a single allocator."""

    def __init__(self, allocator: str):
        self.allocator = allocator
        self.metrics: dict[str, dict] = {}
        self.data: dict[str, Any] = {}
        self.metrics[BY_ALLOC_COUNT] = {}
        self.metrics[BY_FRAGMENTATION] = {}
        self.metrics[BY_ALLOC_SIZE] = {}

    def collect(self):
        """Builds and runs all categories of an allocator benchmark."""
        pw_root = os.getenv('PW_ROOT')
        if not pw_root:
            raise RuntimeError('PW_ROOT is not set')
        benchmark = f'{self.allocator}_benchmark'
        label = f'//pw_allocator/benchmarks:{benchmark}'
        subprocess.run([BAZEL, 'build', label], cwd=pw_root)
        p1 = subprocess.Popen(
            [BAZEL, 'run', label], cwd=pw_root, stdout=subprocess.PIPE
        )
        p2 = subprocess.Popen(
            [
                'python',
                'pw_tokenizer/py/pw_tokenizer/detokenize.py',
                'base64',
                f'bazel-bin/pw_allocator/benchmarks/{benchmark}#.*',
            ],
            cwd=pw_root,
            stdin=p1.stdout,
            stdout=subprocess.PIPE,
        )
        p1.stdout.close()
        output = p2.communicate()[0]
        lines = [
            line[18:] for line in output.decode('utf-8').strip().split('\n')
        ]
        for _, data in json.loads('\n'.join(lines)).items():
            self.data = data
            self.collect_category(BY_ALLOC_COUNT)
            self.collect_category(BY_FRAGMENTATION)
            self.collect_category(BY_ALLOC_SIZE)

    def collect_category(self, category: str):
        """Runs one category of an allocator benchmark."""
        category_data = self.data[category]
        for name in BUCKETS[category]:
            self.metrics[category][name] = category_data[name]


class BenchmarkSuite:
    """Collection of benchmarks for all supported allocators."""

    def __init__(self):
        self.benchmarks = [Benchmark(allocator) for allocator in ALLOCATORS]

    def collect(self):
        """Builds and runs all allocator benchmarks."""
        for benchmark in self.benchmarks:
            benchmark.collect()

    def write_benchmarks(self, output: IO):
        """Reorganizes benchmark metrics and writes them to the given output."""
        for metric in METRICS:
            self.write_category(output, metric, BY_ALLOC_COUNT)
            self.write_category(output, metric, BY_FRAGMENTATION)
            self.write_category(output, metric, BY_ALLOC_SIZE)

    def write_category(self, output: IO, metric: str, category: str):
        """Writes a single category of benchmarks to the given output."""
        output.write(f'{metric} {category}\t')
        for benchmark in self.benchmarks:
            output.write(f'{benchmark.allocator}\t')
        output.write('\n')

        for bucket in BUCKETS[category]:
            output.write(f'{bucket}\t')
            for benchmark in self.benchmarks:
                output.write(f'{benchmark.metrics[category][bucket][metric]}\t')
            output.write('\n')
        output.write('\n')

    def print_summary(self):
        """Writes selected metrics to stdout."""
        print('\n' + '#' * 80)
        print(f'Results for {BY_ALLOC_COUNT_1K}:')
        sys.stdout.write(' ' * 32)
        for benchmark in self.benchmarks:
            sys.stdout.write(benchmark.allocator.ljust(16))
        sys.stdout.write('\n')

        for metric in METRICS:
            sys.stdout.write(metric.ljust(32))
            for benchmark in self.benchmarks:
                metrics = benchmark.metrics[BY_ALLOC_COUNT][BY_ALLOC_COUNT_1K]
                sys.stdout.write(f'{metrics[metric]}'.ljust(16))
            sys.stdout.write('\n')
        print('#' * 80)


def main() -> int:
    """Builds and runs allocator benchmarks."""
    args = _parse_args()
    suite = BenchmarkSuite()
    suite.collect()
    if args.output:
        with open(Path(args.output), 'w+') as output:
            suite.write_benchmarks(output)
        print(f'\nWrote to {Path(args.output).resolve()}')

    if args.verbose:
        suite.write_benchmarks(sys.stdout)

    suite.print_summary()

    return 0


if __name__ == '__main__':
    main()
