"""Setup script for GPU Memory Footprint Profiler."""

from setuptools import setup, find_packages
from pathlib import Path

# Read the README file
readme_file = Path(__file__).parent / "python" / "README.md"
long_description = readme_file.read_text() if readme_file.exists() else ""

setup(
    name="gpu-memory-footprint-profiler",
    version="1.0.0",
    description="GPU Memory Footprint Profiler",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="GPU Memory Footprint Profiler Contributors",
    url="https://github.com/yourusername/GPUMemoryFootprintProfiler",
    packages=find_packages(where="python"),
    package_dir={"": "python"},
    python_requires=">=3.7",
    install_requires=[],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "pytest-cov>=2.0",
            "black>=21.0",
            "mypy>=0.900",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: System :: Hardware",
    ],
)

