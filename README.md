# Postgres Open Telemetry Extension

The `pgotel` project is a Postgres extension that allows other extensions to export observability signals (metrics, traces, logs, etc) with [Open Telemetry](https://opentelemetry.io/). `pgotel` includes and exposes in a more Postgres-way (and in plain C, not C++) the [Open Telemetry C++ SDK](https://github.com/open-telemetry/opentelemetry-cpp).

## Protocol considerations

The [OpenTelemetry Protocol](https://opentelemetry.io/docs/specs/otlp/) (OTLP in short) defines two possible transports: gRPC and HTTP (supporting both 1.1 and 2.0 for the latter). The HTTP transport was added to OTLP after the gRPC transport with one of the main motivations being that "_gRPC is a relatively big dependency, which some clients are not willing to take. Plain HTTP is a smaller dependency and is built in the standard libraries of many programming languages_" ([source](https://github.com/open-telemetry/oteps/blob/main/text/0099-otlp-http.md)). For this reason, only the HTTP transport has been chosen to be implemented into this extension.

## Build and Install dependencies

```bash
# Install gRPC
git clone --recurse-submodules -b v1.62.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_STANDARD=17 \
    -DABSL_PROPAGATE_CXX_STD=ON \
    ../..
make -j6
sudo make install

# Install Abseil
popd
mkdir -p third_party/abseil-cpp/cmake/build
pushd third_party/abseil-cpp/cmake/build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_STANDARD=17 \
    -DABSL_CXX_STANDARD=17 \
    -DABSL_PROPAGATE_CXX_STD=ON \
    ../..
make -j6
sudo make install
popd

# Rebuild ldconfig cache
sudo ldconfig

# Install Opentelemetry C++ SDK
git clone --recursive https://github.com/open-telemetry/opentelemetry-cpp
cd opentelemetry-cpp
mkdir -p build
pushd build
cmake -DBUILD_TESTING=OFF \
  -DWITH_OTLP_GRPC=ON \
  -DWITH_OTLP_HTTP=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_CXX_STANDARD=17 \
  -DWITH_ABSEIL=ON \
  -DBUILD_SHARED_LIBS=ON ..
make -j6
sudo make install
popd

# Rebuild ldconfig cache
sudo ldconfig
```

## Build Extension

```bash
make USE_PGXS=1
make USE_PGXS=1 install
```

## Run OTEL collector

```bash
cd opentelemetry-cpp
## follow the instructions from here: https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples/otlp
```

## Test extension

For now we have just one function named `pg_otlp` that send a trace to OpenTelemetry collector.

```sql
CREATE EXTENSION pg_otlp;
SELECT pg_otlp_log('localhost:4317', 'teste');
```
