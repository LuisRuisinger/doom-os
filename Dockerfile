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

# --enable-initfini-array: a --without-headers build has no libc to probe, so GCC would silently
# fall back to .ctors, which the linker script does not collect and no global constructor runs.
# --disable-hosted-libstdcxx: only the freestanding, header-only subset of libstdc++ is installed.
# No libc is built: libos implements the Linux ABI and the integrator picks the libc.
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

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGET=x86_64-elf
ARG PREFIX=/opt/cross
ARG CLANG_FORMAT_VERSION=20
ARG NEOVIM_VERSION=0.11.6
ARG RUST_TOOLCHAIN=stable
ARG RUST_TARGET=x86_64-unknown-linux-musl

ENV CARGO_HOME=/opt/cargo
ENV RUSTUP_HOME=/opt/rustup
ENV PATH="${CARGO_HOME}/bin:${PREFIX}/bin:${PATH}"

# libgmp10..libzstd1 are the cross compiler's shared libraries. ripgrep and fd serve the neovim pickers.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgmp10 \
    libmpc3 \
    libmpfr6 \
    libisl23 \
    libzstd1 \
    make \
    cmake \
    ninja-build \
    git \
    gdb \
    wget \
    sudo \
    ca-certificates \
    grub-common \
    grub-pc-bin \
    xorriso \
    mtools \
    qemu-system-x86 \
    qemu-system-gui \
    ripgrep \
    fd-find \
  && rm -rf /var/lib/apt/lists/* \
  && ln -s /usr/bin/fdfind /usr/local/bin/fd

# .clang-format uses AlignFunctionDeclarations, which needs clang-format >= 20; Ubuntu 24.04 ships 18.
RUN wget -qO /usr/share/keyrings/llvm.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.asc] http://apt.llvm.org/noble/ llvm-toolchain-noble-${CLANG_FORMAT_VERSION} main" \
       > /etc/apt/sources.list.d/llvm.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends "clang-format-${CLANG_FORMAT_VERSION}" \
    && ln -sf "/usr/bin/clang-format-${CLANG_FORMAT_VERSION}" /usr/bin/clang-format \
    && rm -rf /var/lib/apt/lists/*

# Must match the host: plugins, parsers and the bytecode cache are bind-mounted from there.
RUN wget -qO- "https://github.com/neovim/neovim/releases/download/v${NEOVIM_VERSION}/nvim-linux-x86_64.tar.gz" \
    | tar -xz -C /usr/local --strip-components=1

RUN wget -qO- https://sh.rustup.rs \
      | sh -s -- -y --no-modify-path \
        --profile minimal \
        --default-toolchain "${RUST_TOOLCHAIN}" \
        --target "${RUST_TARGET}" \
        --component rust-analyzer,rust-src \
    && chown -R ubuntu:ubuntu "${RUSTUP_HOME}" "${CARGO_HOME}"

COPY --from=toolchain ${PREFIX} ${PREFIX}

RUN "${TARGET}-g++" --version \
    && test -f "$("${TARGET}-gcc" -print-libgcc-file-name)" \
    && test -d "${PREFIX}/${TARGET}/include/c++" \
    && test -f "$(rustc --print target-libdir --target "${RUST_TARGET}")/self-contained/libc.a" \
    && rust-analyzer --version \
    && grub-mkrescue --version \
    && qemu-system-x86_64 --version \
    && clang-format --version \
    && nvim --version

# uid 1000 matches the first host user, so bind-mounted files keep their owner.
RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu \
    && chmod 0440 /etc/sudoers.d/ubuntu \
    && mkdir -p /workspace /home/ubuntu/.config /home/ubuntu/.local/share /home/ubuntu/.local/state /home/ubuntu/.cache \
    && chown -R ubuntu:ubuntu /workspace /home/ubuntu

USER ubuntu
WORKDIR /workspace
