# =================================================================================================
# Toolchain builder
#
# Everything here is thrown away except /opt/cross. Building and cleaning up inside a single
# layer keeps the ~5 GB of sources and build trees out of the image that actually ships.
# =================================================================================================

FROM ubuntu:24.04 AS toolchain

ARG DEBIAN_FRONTEND=noninteractive

ARG TARGET=x86_64-elf
ARG PREFIX=/opt/cross
ARG BINUTILS_VERSION=2.46.1
ARG GCC_VERSION=15.3.0

ENV PATH="${PREFIX}/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    bison \
    flex \
    libgmp3-dev \
    libmpc-dev \
    libmpfr-dev \
    libisl-dev \
    libzstd-dev \
    texinfo \
    wget \
    ca-certificates \
    tar \
    xz-utils \
    make \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/cross

# --enable-initfini-array is not optional here. GCC decides between .init_array and the legacy
# .ctors by probing the target libc and binutils at configure time, and a --without-headers cross
# build has no libc to probe, so it silently falls back to .ctors. The linker script collects
# .init_array, so without this flag every global constructor in the image lands in a section
# nothing walks and never runs. config/linker.ld.in asserts .ctors is empty to catch a toolchain
# built without it.
#
# libstdc++ is built freestanding (--disable-hosted-libstdcxx), which installs only the subset
# C++20 requires of a freestanding implementation: <type_traits>, <concepts>, <bit>, <limits>,
# <new>, <exception>, <cstddef>, <cstdint> and friends. All header-only template machinery, so
# the kernel keeps linking with -nostdlib.
RUN wget -q "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz" \
    && wget -q "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz" \
    && tar -xf "binutils-${BINUTILS_VERSION}.tar.xz" \
    && tar -xf "gcc-${GCC_VERSION}.tar.xz" \
    && mkdir build-binutils \
    && cd build-binutils \
    && "../binutils-${BINUTILS_VERSION}/configure" \
      --target="${TARGET}" \
      --prefix="${PREFIX}" \
      --with-sysroot \
      --disable-nls \
      --disable-werror \
    && make -j"$(nproc)" \
    && make install \
    && cd /tmp/cross \
    && mkdir build-gcc \
    && cd build-gcc \
    && "../gcc-${GCC_VERSION}/configure" \
      --target="${TARGET}" \
      --prefix="${PREFIX}" \
      --disable-nls \
      --enable-languages=c,c++ \
      --without-headers \
      --disable-shared \
      --disable-threads \
      --disable-libssp \
      --disable-libgomp \
      --disable-libquadmath \
      --disable-libatomic \
      --enable-initfini-array \
      --disable-hosted-libstdcxx \
      --disable-libstdcxx-verbose \
      --disable-libstdcxx-pch \
    && make -j"$(nproc)" all-gcc \
    && make -j"$(nproc)" all-target-libgcc \
    && make install-gcc \
    && make install-target-libgcc \
    && make -j"$(nproc)" all-target-libstdc++-v3 \
    && make install-target-libstdc++-v3 \
    && cd / \
    && rm -rf /tmp/cross

# =================================================================================================
# No C library
#
# There is deliberately none. Which libc an image carries is the integrator's decision, the same
# way the application and its drivers are: libos implements the x86_64 Linux ABI that a libc sits
# on, and the objects handed to doom_os_application() arrive with whatever they were built
# against. Building one here would only decide it for them.
#
# libstdc++ above is installed for its headers, not its archives. The freestanding subset is
# header-only template machinery the kernel uses; nothing links -lstdc++ or -lsupc++.
# =================================================================================================

# =================================================================================================
# Development image
# =================================================================================================

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

ARG TARGET=x86_64-elf
ARG PREFIX=/opt/cross
ARG CLANG_FORMAT_VERSION=20

ENV TARGET=${TARGET}
ENV PREFIX=${PREFIX}
ENV PATH="${PREFIX}/bin:${PATH}"

# The -dev packages are what the cross compiler links against at run time (libgmp, libmpc,
# libmpfr, libisl, libzstd).
#
# cmake and ninja build the kernel; git is load-bearing rather than a convenience, because
# CMake fetches the Result dependency at configure time. grub, xorriso and mtools turn a
# kernel.elf into a bootable ISO, and qemu runs it. make is not used by the build, but CMake
# defaults to the Makefiles generator when no preset is given and the failure is confusing
# without it.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgmp3-dev \
    libmpc-dev \
    libmpfr-dev \
    libisl-dev \
    libzstd-dev \
    make \
    cmake \
    ninja-build \
    git \
    gdb \
    wget \
    gnupg \
    sudo \
    ca-certificates \
    grub-common \
    grub-pc-bin \
    xorriso \
    mtools \
    qemu-system-x86 \
    qemu-system-gui \
  && rm -rf /var/lib/apt/lists/*

# .clang-format uses AlignFunctionDeclarations, which only exists from clang-format 20 - older
# releases reject the whole file with "unknown key" and format nothing. Ubuntu 24.04 ships 18,
# so take it from apt.llvm.org and pin the version, otherwise the container and the IDE format
# the same file differently.
RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key \
      | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg]" \
       "http://apt.llvm.org/noble/ llvm-toolchain-noble-${CLANG_FORMAT_VERSION} main" \
       > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends "clang-format-${CLANG_FORMAT_VERSION}" \
    && ln -sf "/usr/bin/clang-format-${CLANG_FORMAT_VERSION}" /usr/bin/clang-format \
    && rm -rf /var/lib/apt/lists/*

COPY --from=toolchain /opt/cross /opt/cross

RUN x86_64-elf-gcc --version \
    && test -f "${PREFIX}/${TARGET}/lib/libc.a" \
    && x86_64-elf-g++ --version \
    && grub-mkrescue --version \
    && qemu-system-x86_64 --version \
    && clang-format --version \
    && cmake --version \
    && ninja --version

# =================================================================================================
# Unprivileged user
#
# Running as root meant every file the container touched in the bind-mounted workspace came back
# owned by root - build output, and any source file created inside the container. The stock
# ubuntu user is uid/gid 1000, which is the first user on a typical Linux host, so ownership
# lines up without remapping. sudo is there for the occasional ad-hoc install.
# =================================================================================================

RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu \
    && mkdir -p /workspace \
    && chown ubuntu:ubuntu /workspace

USER ubuntu

WORKDIR /workspace
