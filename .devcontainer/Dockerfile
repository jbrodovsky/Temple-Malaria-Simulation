FROM ubuntu:24.04

# Set up non-interactive frontend for apt
ENV DEBIAN_FRONTEND=noninteractive

# Update and install system-level dependencies for TMS
RUN apt-get update && apt-get install -y \
    autoconf \
    bison \
    build-essential \
    cmake \
    flex \
    git \
    postgresql \
    software-properties-common

# Install vcpkg dependencies
RUN apt-get install -y \
    curl \
    tar \
    pkg-config \
    unzip \
    zip
# Install vcpgk
RUN git clone https://github.com/microsoft/vcpkg.git /usr/local/vcpkg
# Bootstrap vcpkg
RUN /usr/local/vcpkg/bootstrap-vcpkg.sh
# Add vcpkg to PATH
ENV PATH="/usr/local/vcpkg:${PATH}"
# Set environment variables for vcpkg integration
ENV VCPKG_ROOT=/usr/local/vcpkg
ENV CMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake