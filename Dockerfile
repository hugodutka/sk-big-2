FROM gcc:10
RUN apt-get update && apt-get install telnet -y
ARG cachebust=1
RUN git clone https://github.com/hugodutka/sk-big-2
RUN cd sk-big-2 && make
