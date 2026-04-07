FROM debian:trixie

RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    vulkan-tools \
    libglfw3-dev \
    libvulkan-dev \
    libxkbcommon-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxrandr-dev \
    libxi-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
