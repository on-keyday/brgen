#license
FROM ubuntu:devel

RUN mkdir -p /workspace

WORKDIR /workspace

# if you are not in Japan region, you should comment out this line, or change `ftp.jaist.ac.jp/pub/Linux` to your region millor server address.
RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y\
    clang-15\
    libc++-15-dev\
    golang-go\
    lldb-15\
    lld\
    git\
    curl

RUN apt-get update && \
    apt-get install -y\
    ninja-build\
    cmake

RUN ln -s /lib/llvm-15/bin/clang++ /bin/clang++
RUN ln -s /lib/llvm-15/bin/clang /bin/clang
RUN ln -s /bin/lldb-15 /bin/lldb
RUN ln -s /lib/llvm-15/lib/libc++abi.so.1.0 /lib/llvm-15/lib/libc++abi.so
RUN unlink /usr/bin/ld
RUN ln -s /bin/lld /usr/bin/ld

