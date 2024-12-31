cd cpp && \
mkdir -p build && \
cd build && \
cmake .. && \
make -j $(nproc) && \
make install && \
cd ../../python && \
pip3 install . --break-system-packages