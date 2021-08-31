# pg_otlp

POC of a PostgreSQL extension to interact with OpenTelemetry


## Build and Install dependencies

```bash
# Install gRPC
git clone --recurse-submodules -b v1.38.0 https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBUILD_SHARED_LIBS=ON \
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
cmake -DBUILD_TESTING=OFF -DWITH_LOGS_PREVIEW=ON -DWITH_OTLP=ON \
  -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
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
docker run --rm -it -p 4317:4317 -p 55681:55681 -v $(pwd)/examples/otlp:/cfg otel/opentelemetry-collector:0.19.0 --config=/cfg/opentelemetry-collector-config/config.dev.yaml
```

## Test extension

For now we have just one function named `pg_otlp` that send a trace to OpenTelemetry collector.

```sql
CREATE EXTENSION pg_otlp;
SELECT pg_otlp('localhost:4317');
```
