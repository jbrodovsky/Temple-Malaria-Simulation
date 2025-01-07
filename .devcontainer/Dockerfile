FROM ubuntu:24.04

# Set up non-interactive frontend for apt
ENV DEBIAN_FRONTEND=noninteractive

# Update and install system-level dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgsl-dev \
    libyaml-cpp-dev \
    libfmt-dev \
    wget \
    git \
    software-properties-common && \
    rm -rf /var/lib/apt/lists/*

# Configure PostgreSQL repository and install PostgreSQL
RUN sh -c 'echo "deb http://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" > /etc/apt/sources.list.d/pgdg.list' && \
    wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - && \
    apt-get update && apt-get install -y \
    postgresql-10 \
    libpq-dev

# Clone, build, and install libpqxx
RUN git clone https://github.com/jtv/libpqxx.git /tmp/libpqxx && \
    cd /tmp/libpqxx && \
    git checkout 7.8.1 && \
    ./configure --disable-documentation && \
    make && \
    make install && \
    rm -rf /tmp/libpqxx

# Create the build directory
WORKDIR /project
RUN mkdir -p build
