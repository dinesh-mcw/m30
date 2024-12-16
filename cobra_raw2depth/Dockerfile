FROM rikorose/gcc-cmake:gcc-11
RUN apt-get update -y
RUN apt-get upgrade -y
RUN apt-get install rsyslog -y
# Disables kernel level monitoring cause its a Docker container...
RUN sed -i '/imklog/s/^/#/' /etc/rsyslog.conf
CMD rsyslogd -n
RUN git clone https://github.com/google/googletest.git -b release-1.11.0
RUN  cd googletest && mkdir build && cd build
RUN ls -l
WORKDIR /googletest/build
RUN cmake ..
RUN make
RUN make install
