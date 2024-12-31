cd cpp && \
mkdir -p build && \
cd build && \
cmake .. && \
make -j "$(nproc)" && \
make install && \
cd ../python && \
RUN cd /tmp/python && \
pip install . --break-system-packages