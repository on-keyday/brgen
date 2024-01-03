#license
FROM ubuntu:devel

RUN mkdir -p /workspace

WORKDIR /workspace

# if you are not in Japan region, you should comment out this line, or change `ftp.jaist.ac.jp/pub/Linux` to your region millor server address.
RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

RUN /bin/bash -c "$(curl https://apt.llvm.org/llvm.sh)"


RUN apt-get update && \
    apt-get install -y\
    clang\
    libc++-dev\
    golang-go\
    lldb\
    liblldb-dev\
    lld\
    git\
    curl

RUN apt-get update && \
    apt-get install -y\
    ninja-build\
    cmake
RUN apt-get update && \
    apt-get install -y\
    vim

# RUN ln -s /lib/llvm-15/bin/clang++ /bin/clang++
# RUN ln -s /lib/llvm-15/bin/clang /bin/clang
# RUN ln -s /bin/lldb-15 /bin/lldb
# RUN ln -s /lib/llvm-15/lib/libc++abi.so.1.0 /lib/llvm-15/lib/libc++abi.so
RUN unlink /usr/bin/ld
RUN ln -s /bin/lld /usr/bin/ld

RUN apt-get update && \
    apt-get install -y\
    unzip

RUN curl https://github.com/lldb-tools/lldb-mi/archive/refs/heads/main.zip \
    -o /workspace/lldb-mi.zip -L


RUN unzip /workspace/lldb-mi.zip -d /workspace
RUN rm /workspace/lldb-mi.zip

RUN (cd /workspace/lldb-mi-main;cmake -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang .)
RUN (cd /workspace/lldb-mi-main;cmake --build .)
RUN cp /workspace/lldb-mi-main/src/lldb-mi /bin/lldb-mi
