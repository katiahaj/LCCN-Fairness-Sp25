FROM ubuntu:22.04
	
	ENV DEBIAN_FRONTEND=noninteractive
	RUN apt-get update && \
	    apt-get install -y --no-install-recommends \
	      ca-certificates \
	      curl \
	      bzip2 \
	      build-essential \
	      python3 \
	      python3-dev \
	      cmake \
	      pkg-config && \
	    rm -rf /var/lib/apt/lists/*
	
	RUN apt-get update && apt-get install -y python3-pip && \
	    rm -rf /var/lib/apt/lists/*
	RUN pip3 install cppyy
	
	ARG NS3_VERSION=3.43
	RUN curl -fsSL https://www.nsnam.org/releases/ns-allinone-${NS3_VERSION}.tar.bz2 \
	     -o /tmp/ns-allinone-${NS3_VERSION}.tar.bz2 && \
	    tar xjf /tmp/ns-allinone-${NS3_VERSION}.tar.bz2 -C /opt && \
	    rm /tmp/ns-allinone-${NS3_VERSION}.tar.bz2
	
	WORKDIR /opt/ns-allinone-${NS3_VERSION}/ns-${NS3_VERSION}
	RUN ./ns3 configure --enable-examples --enable-tests --enable-python-bindings && \
	    ./ns3 build
	
	COPY script.cc scratch/

	ENV NS3_ROOT=/opt/ns-allinone-${NS3_VERSION}/ns-${NS3_VERSION}
	ENV PATH="${NS3_ROOT}:$PATH"
	
	WORKDIR /workspace
	CMD ["bash"]