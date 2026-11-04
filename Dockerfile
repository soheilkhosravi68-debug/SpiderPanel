# syntax=docker/dockerfile:1

########## build stage ##########
# Compiles MTProxy and installs Python dependencies in a throwaway layer so the
# compiler toolchain never ships in the final image.
FROM python:3.13-slim AS builder

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN apt-get update \
    && apt-get upgrade -y --no-install-recommends \
    && apt-get install -y --no-install-recommends \
       build-essential \
       git \
       curl \
       ca-certificates \
       libssl-dev \
       zlib1g-dev \
       pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Build MTProxy where supported and leave the binary in /out.
RUN set -eux; \
    mkdir -p /out; \
    arch="$(dpkg --print-architecture)"; \
    case "$arch" in \
      amd64) \
        git clone --depth 1 \
          https://github.com/TelegramMessenger/MTProxy.git /tmp/MTProxy; \
        if ! make -C /tmp/MTProxy; then \
          make -C /tmp/MTProxy clean || true; \
          echo "ERROR: MTProxy compilation failed on AMD64."; \
          exit 1; \
        fi; \
        install -m 0755 \
          /tmp/MTProxy/objs/bin/mtproto-proxy \
          /out/mtproto-proxy; \
        rm -rf /tmp/MTProxy \
        ;; \
      arm64) \
        echo "ARM64 detected: upstream MTProxy build skipped."; \
        echo "The panel will build, but MTProxy is not available."; \
        ;; \
      *) \
        echo "Unsupported architecture: $arch"; \
        exit 1 \
        ;; \
    esac

# Python dependencies in an isolated virtualenv that is copied to the runtime.
COPY requirements.txt /tmp/requirements.txt
RUN python -m venv /opt/venv \
    && /opt/venv/bin/pip install --upgrade pip \
    && /opt/venv/bin/pip install --no-cache-dir -r /tmp/requirements.txt

########## runtime stage ##########
FROM python:3.13-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PATH="/opt/venv/bin:$PATH"

# Apply distributor security updates, then install only the runtime tools the
# panel actually needs. The curl CLI and the build toolchain stay out of the
# final image because they are not used at runtime.
RUN apt-get update \
    && apt-get upgrade -y --no-install-recommends \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Prebuilt runtime artifacts from the build stage.
COPY --from=builder /opt/venv /opt/venv
COPY --from=builder /out/ /usr/local/bin/

# Application source
COPY . .

# Fail early if the main Python modules contain syntax errors.
RUN python -m py_compile main.py app.py vercel_relay.py \
    && mkdir -p /app/data

EXPOSE 8080
EXPOSE 443

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8080"]
