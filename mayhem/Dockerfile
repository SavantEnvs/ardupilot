# syntax=docker/dockerfile:1.7
#
# Commit image for ArduPilot (SITL board, waf). Built on the org base; mayhem/build.sh builds the
# AFL-instrumented ASan+UBSan fuzz targets and the upstream unit tests, mayhem/test.sh runs the tests.
# Upstream's root .dockerignore drops .git and most of the tree; mayhem/Dockerfile.dockerignore
# (BuildKit's per-Dockerfile ignore file) is used instead.
#
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile --build-arg COMMIT=$(git rev-parse HEAD) -t ardupilot .
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# waf's code generators (pymavlink mavgen, dronecan_dsdlc) need python modules: a pinned venv under
# /opt/toolchains/python (empy must stay 3.x: 4.x breaks the generators).
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends python3-venv \
 && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/toolchains/python \
 && /opt/toolchains/python/bin/pip install --no-cache-dir \
      empy==3.3.4 future==1.0.0 lxml==6.1.3 pexpect==4.9.0 ptyprocess==0.7.0 setuptools==84.2.1 \
 && chmod -R a+rX /opt/toolchains/python

USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# The git submodules the SITL build needs (waf, mavlink+pymavlink, DroneCAN, lwip, littlefs, gtest,
# picolibc), each fetched at the commit this tree's gitlink pins, into a read-only cache
# (SPEC §6.2 item 19). build.sh copies them into modules/, so every rebuild works offline.
USER root
RUN bash /mayhem/mayhem/fetch-modules.sh /mayhem /opt/toolchains/deps/ardupilot-modules/src
USER mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
