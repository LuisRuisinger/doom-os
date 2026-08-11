FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

ARG TARGET=x86_64-elf
ARG PREFIX=/opt/cross
ARG BINUTILS_VERSION=2.46.1
ARG GCC_VERSION=15.3.0

ENV TARGET=${TARGET}
ENV PREFIX=${PREFIX}
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
    git \
    bear \
    clang-format \
    grub-common \
    grub-pc-bin \
    xorriso \
    mtools \
    qemu-system-x86 \
    qemu-system-gui \
    gdb \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/cross

RUN wget "https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz" \
    && wget "https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz" \
    && tar -xf "binutils-${BINUTILS_VERSION}.tar.xz"  \
    && tar -xf "gcc-${GCC_VERSION}.tar.xz"

RUN mkdir build-binutils  \
    && cd build-binutils \
    && "../binutils-${BINUTILS_VERSION}/configure"  \
      --target="${TARGET}" \
      --prefix="${PREFIX}" \
      --with-sysroot \
      --disable-nls \
      --disable-werror \
    && make -j"$(nproc)" \
    && make install

# libstdc++ is built in freestanding mode (--disable-hosted-libstdcxx). That installs only
# the subset C++20 requires of a freestanding implementation - <type_traits>, <concepts>,
# <bit>, <limits>, <new>, <exception>, <cstddef>, <cstdint> and friends. All of it is
# header-only template machinery: nothing allocates, nothing calls into an OS, and nothing is
# emitted unless used, so the kernel keeps linking with -nostdlib.
RUN mkdir build-gcc \
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
      --disable-hosted-libstdcxx \
      --disable-libstdcxx-verbose \
      --disable-libstdcxx-pch \
    && make -j"$(nproc)" all-gcc \
    && make -j"$(nproc)" all-target-libgcc \
    && make install-gcc \
    && make install-target-libgcc \
    && make -j"$(nproc)" all-target-libstdc++-v3 \
    && make install-target-libstdc++-v3

RUN x86_64-elf-gcc --version \
    && x86_64-elf-g++ --version \
    && grub-mkrescue --version \
    && qemu-system-x86_64 --version

WORKDIR /workspace