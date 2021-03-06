FROM centos:8 as builder

<<<<<<< HEAD
RUN yum update -y
RUN yum install -y tar git gcc-c++ gcc make cmake python3 bzip2

=======
# ----------- Get packages -----------
RUN yum update -y
RUN yum install -y tar git gcc-c++ gcc make cmake python3 bzip2

# ----------- Get nGen -----------
>>>>>>> 1099b80e3113e24e26abb0a418bb60351d625130
RUN git clone https://github.com/NOAA-OWP/ngen.git 

WORKDIR ngen

ENV CXX=/usr/bin/g++

RUN git submodule update --init --recursive -- test/googletest

RUN curl -L -O https://dl.bintray.com/boostorg/release/1.72.0/source/boost_1_72_0.tar.bz2

RUN tar -xjf boost_1_72_0.tar.bz2

ENV BOOST_ROOT="boost_1_72_0"

WORKDIR /ngen

RUN cmake -B /ngen -S .

RUN cmake --build /ngen --target ngen

WORKDIR /ngen/

<<<<<<< HEAD
=======
# ----------- Run nGEn -----------
>>>>>>> 1099b80e3113e24e26abb0a418bb60351d625130
CMD ./ngen data/catchment_data.geojson "" data/nexus_data.geojson "" data/example_realization_config.json
