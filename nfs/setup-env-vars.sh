grpc="/unsup/grpc-1.0.0"
pbuf="/unsup/protobuf-3.0.2"
thrift="/unsup/thrift-0.9.3"

pc="lib/pkgconfig"

export PATH="$grpc/bin:$pbuf/bin:$thrift/bin:$PATH"
export LD_LIBRARY_PATH="$grpc/lib:$pbuf/lib:$thrift/lib"
export PKG_CONFIG_PATH="$grpc/$pc:$pbuf/$pc:$thrift/$pc"
