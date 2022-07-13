from gcc:12.1.0

run apt-get -y update
run apt-get -y install cmake
run apt-get -y install libboost-context-dev

run apt-get -y install time # for benchamrks

workdir /home

cmd echo "*** Welcome to cpp-effects container (gcc 12.1.0) ***"; bash

# sudo docker build -t cppeff .
# docker run -it --rm -v $(pwd):/home cppeff