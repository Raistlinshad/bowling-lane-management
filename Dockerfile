# server/Dockerfile
FROM debian:bullseye-slim

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV DISPLAY=:1

# Install Qt5 and development tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    qtbase5-dev \
    qtbase5-dev-tools \
    libqt5widgets5 \
    libqt5network5 \
    libqt5sql5 \
    libqt5sql5-sqlite \
    qt5-qmake \
    pkg-config \
    net-tools \
    procps \
    xvfb \
    x11vnc \
    fluxbox \
    novnc \
    websockify \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source files
COPY . .
RUN ls -la /app
RUN find /app -name "CMakeLists.txt"

# Create build directory and build
RUN mkdir -p build && \
    cd build && \
    cmake .. && \
    make -j$(nproc)

# Create data directory
RUN mkdir -p /app/data

# Expose ports
EXPOSE 50005 5900 6080

# Create startup script
RUN echo '#!/bin/bash\n\
Xvfb :1 -screen 0 1024x768x24 &\n\
export DISPLAY=:1\n\
fluxbox &\n\
x11vnc -display :1 -forever -nopw -create &\n\
websockify --web /usr/share/novnc 6080 localhost:5900 &\n\
cd build\n\
./Canadian5PinBowling\n\
' > /start.sh && chmod +x /start.sh

CMD ["/start.sh"]