FROM ubuntu:24.04
RUN apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install --no-install-recommends \
    --no-install-suggests \
    apt-transport-https \
    apt-utils \
    automake \
    build-essential \
    ca-certificates \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    colordiff \
    curl \
    g++ \
    gawk \
    gcc \
    gdb \
    git \
    just \
    less \
    libabsl-dev \
    libbenchmark-dev \
    libcurl4-openssl-dev \
    libgmock-dev \
    libgrpc++-dev \
    libgtest-dev \
    libprotobuf-dev \
    libssl-dev \
    libtool \
    llvm \
    lsb-release \
    make \
    ninja-build \
    nlohmann-json3-dev \
    patch \
    pkg-config \
    protobuf-compiler \
    protobuf-compiler-grpc \
    python3 \
    python3-dev \
    python3-pip \
    strace \
    tar \
    tzdata \
    unzip \
    vim \
    wget \
    zip \
    zlib1g-dev
# Install Python packages used in the integration tests.
RUN update-alternatives --install /usr/bin/python python $(which python3) 10
RUN apt update && apt install python3-setuptools python3-wheel
# The Cloud Pub/Sub emulator needs Java :shrug:
RUN apt update && (apt install -y openjdk-11-jre || apt install -y openjdk-9-jre)
WORKDIR /var/tmp/
RUN curl -L \
  "https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-cli-474.0.0-linux-x86_64.tar.gz" \
  -o "google-cloud-cli-474.0.0-linux-x86_64.tar.gz"
RUN tar x -C /usr/local -f "google-cloud-cli-474.0.0-linux-x86_64.tar.gz"
RUN /usr/local/google-cloud-sdk/bin/gcloud --quiet components install \
  beta bigtable cbt cloud-spanner-emulator
RUN mkdir -p /var/tmp/pkgconf && cd /var/tmp/pkgconf && \
    rm -f /usr/bin/pkgconf /usr/bin/pkg-config && \
    curl -fsSL https://distfiles.ariadne.space/pkgconf/pkgconf-2.2.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr -with-pkg-config-dir=/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig && \
    make -j ${NCPU:-8} && \
    make install && \
    ldconfig && cd /var/tmp && rm -fr build && \
    ln -s /usr/bin/pkgconf /usr/bin/pkg-config
RUN mkdir -p /var/tmp/crc32c && cd /var/tmp/crc32c && curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-8} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-8} && \
    ldconfig
RUN mkdir -p /var/tmp/opentelemetry-cpp && cd /var/tmp/opentelemetry-cpp && curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.18.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -DOPENTELEMETRY_ABI_VERSION_NO=2 \
        -S . -B cmake-out && \
        cmake --build cmake-out --target install -- -j ${NCPU:-8} && \
        ldconfig
RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.24.1/bazelisk-linux-amd64" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel

RUN ln -fs /usr/share/zoneinfo/Europe/Warsaw /etc/localtime && \
      dpkg-reconfigure -f noninteractive tzdata

# TODO: move it to the top
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get -y install \
    ccache \
    clangd
