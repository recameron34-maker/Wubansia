"""Wubansia sound tools: 48 kHz DSP helpers, recipes, rendering and sample QA (see README.md).

Importing the package pins numpy's SIMD level on x86_64 Linux, the render host for masters (ADR 0007): numpy picks its
kernels per CPU at runtime (AVX2, AVX-512), and some of them round the last bit differently, so the same pins gave
different bytes on an AVX-512 and an AVX2 machine. With NPY_ENABLE_CPU_FEATURES=X86_V2 only the baseline kernels run,
on any x86-64-v2 CPU. It must be set before numpy loads: import tools.sound first (render.py does, and so does unittest
discovery, which imports this package before the test modules). A value already in the environment wins.

OpenBLAS, which numpy's matrix products (`@`, `np.dot`) call, also picks its kernels per CPU: the FDN reverbs' 8 x 8
mixing matrix gave a different ContextLoopWet with the Haswell and SkylakeX kernels. OPENBLAS_CORETYPE=Prescott pins
its plain SSE3 kernels, which every x86-64 CPU runs, the same way.
"""
import os
import platform
import sys

SIMD_PIN = 'X86_V2'
BLAS_PIN = 'Prescott'


def _pin_simd():
    if sys.platform.startswith('linux') and platform.machine() == 'x86_64' \
            and 'NPY_DISABLE_CPU_FEATURES' not in os.environ:
        os.environ.setdefault('NPY_ENABLE_CPU_FEATURES', SIMD_PIN)
    if sys.platform.startswith('linux') and platform.machine() == 'x86_64':
        os.environ.setdefault('OPENBLAS_CORETYPE', BLAS_PIN)


_pin_simd()
