FROM alpine:latest

RUN apk add --no-cache \
    doxygen \
    graphviz \
    ttf-freefont \
    inotify-tools \
    git

WORKDIR /theme
RUN git clone https://github.com/jothepro/doxygen-awesome-css.git .

WORKDIR /code

COPY run_doxygen.sh /usr/local/bin/run_docs
RUN chmod +x /usr/local/bin/run_docs

ENTRYPOINT ["/usr/local/bin/run_docs"]
