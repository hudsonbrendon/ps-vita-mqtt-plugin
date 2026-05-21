FROM vitasdk/vitasdk:latest

# vitasdk base already ships arm-vita-eabi-gcc, vita-mksfoex, vita-make-fself,
# vita-elf-create, vita-pack-vpk in /usr/local/vitasdk/bin.
# Add the bits Makefile needs.
RUN apk add --no-cache make bash

WORKDIR /work
ENV VITASDK=/usr/local/vitasdk
ENV PATH=$VITASDK/bin:$PATH
