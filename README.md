# pg_otlp

POC of a PostgreSQL extension to interact with OpenTelemetry


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
