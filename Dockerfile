FROM gcc:10
RUN apt-get update && apt-get install telnet -y
RUN useradd -m -u 501 hugodutka
USER hugodutka
WORKDIR /home/hugodutka
ARG cachebust=1
RUN git clone https://github.com/hugodutka/sk-big-2
RUN cd sk-big-2 && bash release.sh && tar xf zadanie2.tgz && cd zadanie2 && make && cp radio-* ../
RUN mkdir -p /home/hugodutka/data
