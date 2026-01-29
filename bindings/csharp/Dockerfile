# Dockerfile for C# bindings CI testing
FROM mcr.microsoft.com/dotnet/sdk:8.0

# Install C build tools for native library compilation
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy the entire liblpm project
COPY . /build/

# Initialize submodules and build liblpm native library
RUN git config --global --add safe.directory /build && \
    if [ -f .gitmodules ]; then git submodule update --init --recursive; fi && \
    mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -GNinja .. && \
    ninja && \
    ninja install && \
    ldconfig

# Copy native library to runtimes directory
RUN mkdir -p /build/bindings/csharp/runtimes/linux-x64/native && \
    cp /build/build/liblpm.so /build/bindings/csharp/runtimes/linux-x64/native/

# Build and test C# bindings
WORKDIR /build/bindings/csharp

# Restore dependencies
RUN dotnet restore

# Build in Release mode
RUN dotnet build --configuration Release --no-restore

# Run tests
RUN dotnet test --configuration Release --no-build --verbosity normal

# Default command: run tests
CMD ["dotnet", "test", "--configuration", "Release", "--verbosity", "normal"]
