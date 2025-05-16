.RECIPEPREFIX:=>


#[set as required]
INSTALL_DIR=/usr/local/lib
INCLUDE_INSTALL_DIR=/usr/local/include
LD_DIR=/etc/ld.so.conf.d

CC=gcc
CFLAGS=
CFLAGS_TEST=-ggdb3 -O0
WARN_OPTS=-Wall -Wextra
LDFLAGS=-lcmore


#[build constants]
LIB_DIR=./src/lib
TEST_DIR=./src/test
BUILD_DIR=${shell pwd}/build
PACKAGE_DIR=./package


#[installation constants]
SHARED=libmcry.so
STATIC=libmcry.a
HEADER=memcry.h


#[set build options]
ifeq ($(build),debug)
	CFLAGS      += -O0 -ggdb3 -fsanitize=address -DDEBUG
	CFLAGS_TEST += -DDEBUG
	LDFLAGS     += -static-libasan
else
	CFLAGS += -O3 -flto
endif


#[set static analysis options]
ifeq ($(fanalyzer),true)
	CFLAGS += -fanalyzer
endif


#[process targets]
.PHONY prepare:
> mkdir -p ${BUILD_DIR}/test ${BUILD_DIR}/lib ${PACKAGE_DIR}

test: shared
> $(MAKE) -C ${TEST_DIR} tests CC='${CC}' _CFLAGS='${CFLAGS_TEST}' \
		                       _WARN_OPTS='${WARN_OPTS}' \
                               BUILD_DIR='${BUILD_DIR}/test' \
                               LIB_BIN_DIR='${BUILD_DIR}/lib'

all: shared static

shared:
> $(MAKE) -C ${LIB_DIR} shared CC='${CC}' _CFLAGS='${CFLAGS} -fPIC' \
	                           _WARN_OPTS='${WARN_OPTS}' \
							   _LDFLAGS='${LDFLAGS}' \
	                           BUILD_DIR='${BUILD_DIR}/lib'

static:
> $(MAKE) -C ${LIB_DIR} static CC='${CC}' _CFLAGS='${CFLAGS}' \
	                           _WARN_OPTS='${WARN_OPTS}' \
	                           _LDFLAGS='${LDFLAGS}' \
	                           BUILD_DIR='${BUILD_DIR}/lib'

clean:
> $(MAKE) -C ${TEST_DIR} clean BUILD_DIR='${BUILD_DIR}/test'
> $(MAKE) -C ${LIB_DIR} clean BUILD_DIR='${BUILD_DIR}/lib'
> -rm ${PACKAGE_DIR}/*

install:
> mkdir -pv ${INSTALL_DIR}
> cp -v ${BUILD_DIR}/lib/${SHARED} ${INSTALL_DIR}
> cp -v ${BUILD_DIR}/lib/${STATIC} ${INSTALL_DIR}
> mkdir -pv ${INCLUDE_INSTALL_DIR}
> cp -v ${LIB_DIR}/${HEADER} ${INCLUDE_INSTALL_DIR}
> echo "${INSTALL_DIR}" > ${LD_DIR}/90memcry.conf
> ldconfig

uninstall:
> -rm -v ${INSTALL_DIR}/{${SHARED},${STATIC}}
> -rm -v ${INCLUDE_INSTALL_DIR}/${HEADER}
> -rm ${LD_DIR}/90memcry.conf
> ldconfig

package: all
> -cp ${BUILD_DIR}/lib/${SHARED} ${PACKAGE_DIR}
> -cp ${BUILD_DIR}/lib/${STATIC} ${PACKAGE_DIR}
> -cp ${LIB_DIR}/memcry.h ${PACKAGE_DIR}
> -tar cvjf ${PACKAGE_DIR}/memcry.tar.bz2 ${PACKAGE_DIR}/*
