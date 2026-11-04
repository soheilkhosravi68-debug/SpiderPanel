# syntax=docker/dockerfile:1

########## build stage ##########
# Compiles MTProxy and installs Python dependencies in a throwaway layer so the
# compiler toolchain never ships in the final image.
#
# Alpine (musl) is used instead of Debian: the Debian slim base carries
# glibc/curl advisories with no fixed version published for Debian 12/13/14,
# and Debian's libcurl3t64-gnutls maps to the curl source package. Alpine 3.24
# ships patched musl and curl (8.23.0-r0) closures, and every pinned Python
# dependency publishes a musllinux wheel for cp313 x86_64.
FROM python:3.13-alpine AS builder

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

RUN apk upgrade --no-cache \
    && apk add --no-cache \
       build-base \
       linux-headers \
       openssl-dev \
       zlib-dev \
       pkgconf \
       git \
       curl \
       ca-certificates

# Build MTProxy where supported and leave the binary in /out.
RUN set -eux; \
    mkdir -p /out; \
    arch="$(apk --print-arch)"; \
    case "$arch" in \
      x86_64) \
        git clone --depth 1 \
          https://github.com/TelegramMessenger/MTProxy.git /tmp/MTProxy; \
        if ! make -C /tmp/MTProxy; then \
          make -C /tmp/MTProxy clean || true; \
          echo "ERROR: MTProxy compilation failed on x86_64."; \
          exit 1; \
        fi; \
        install -m 0755 \
          /tmp/MTProxy/objs/bin/mtproto-proxy \
          /out/mtproto-proxy; \
        rm -rf /tmp/MTProxy \
        ;; \
      aarch64) \
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
FROM python:3.13-alpine

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PATH="/opt/venv/bin:$PATH"

# Apply distributor security updates, then install only the runtime tools the
# panel actually needs (`git` backs the panel self-update endpoint). The build
# toolchain, curl CLI and compiler headers stay out of the final image.
RUN apk upgrade --no-cache \
    && apk add --no-cache \
       ca-certificates \
       git \
    && rm -rf /var/cache/apk/*

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
