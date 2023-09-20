#license
FROM ubuntu:devel

RUN mkdir -p /workspace

WORKDIR /workspace

# if you are not in Japan region, you should comment out this line, or change `ftp.jaist.ac.jp/pub/Linux` to your region millor server address.
RUN sed -i 's@archive.ubuntu.com@ftp.jaist.ac.jp/pub/Linux@g' /etc/apt/sources.list

RUN apt-get update && \
    apt-get install -y \
    libunwind-dev\
    python3-lldb-15

RUN apt-get update && \
    apt-get  install -y\
    clang-15\
    libc++-15-dev\
    lldb-15\
    liblldb-15-dev\
    lld\
    g++-11

RUN apt-get update && \
    apt-get install -y\
    ninja-build\
    cmake\
    vim\
    unzip\
    curl\
    libpthread-stubs0-dev\
    libssl-dev

RUN apt-get install -y \
    seq-gen\
    net-tools\
    iputils-ping
RUN apt-get update && \
    apt-get install -y\
    zlib1g\
    git

RUN ln -s /lib/llvm-15/bin/clang++ /bin/clang++
RUN ln -s /lib/llvm-15/bin/clang /bin/clang
RUN ln -s /bin/lldb-15 /bin/lldb
RUN ln -s /lib/llvm-15/lib/libc++abi.so.1.0 /lib/llvm-15/lib/libc++abi.so
RUN ln -s /usr/bin/lldb-server-15 /usr/bin/lldb-server-15.0.7
RUN ln -s /bin/g++-11 /bin/g++
RUN unlink /usr/bin/ld
RUN ln -s /bin/lld /usr/bin/ld

RUN curl https://github.com/lldb-tools/lldb-mi/archive/refs/heads/main.zip \
    -o /workspace/lldb-mi.zip -L

RUN unzip /workspace/lldb-mi.zip -d /workspace
RUN rm /workspace/lldb-mi.zip

RUN (cd /workspace/lldb-mi-main;cmake -G Ninja .)
RUN (cd /workspace/lldb-mi-main;cmake --build .)
RUN cp /workspace/lldb-mi-main/src/lldb-mi /bin/lldb-mi

RUN apt-get update && \
    apt-get install -y\
    golang-go

#COPY ./src/ /usr/utilsdev/workspace/src/
#COPY ./build /usr/utilsdev/workspace/build
#COPY ./CMakeLists.txt  /usr/utilsdev/workspace/CMakeLists.txt
#COPY ./.clang-format /usr/utilsdev/workspace/.clang-format


CMD /bin/bash
