FROM python:3.13-slim-trixie AS toolchain

RUN python -c "from pathlib import Path; p=Path('/etc/apt/sources.list.d/debian.sources'); p.write_text(p.read_text().replace('http://deb.debian.org', 'https://deb.debian.org'))" \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
       g++ clang cmake ninja-build git file binutils pkg-config ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work

FROM toolchain AS validation
RUN python -m pip install --no-cache-dir numpy

FROM validation AS runtime
RUN python -c "from pathlib import Path; p=Path('/etc/apt/sources.list.d/debian.sources'); p.write_text(p.read_text().replace('http://deb.debian.org', 'https://deb.debian.org'))" \
    && apt-get update \
    && apt-get install -y --no-install-recommends libvulkan1 mesa-vulkan-drivers \
    && rm -rf /var/lib/apt/lists/*
RUN python -m pip install --no-cache-dir 'vapoursynth==81'
