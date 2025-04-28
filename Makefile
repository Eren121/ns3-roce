# Docker image tag
docker_tag = hpcc

# When running image, avoid creating files as root
docker_user ?= -u $(shell id -u):$(shell id -g)

# When running image, mount git directory in the container in /app
docker_mount = --mount type=bind,src=$(shell pwd),dst=/app

# If set, flag to run the docker container as interactive
docker_interactive ?= -it

# When running container, environment variables
docker_env = -e NS_LOG='$(NS_LOG)' -e CXXFLAGS='-Wall'

docker_extra =
docker_wd = 

# Command to run the docker container
docker_run ?= docker run --rm \
	$(docker_interactive) \
	$(docker_mount) \
	$(docker_user) \
	$(docker_env) \
	$(docker_extra) \
	$(docker_wd) \
	$(docker_tag)

# Don't run programs via docker if we are already in container
ifneq ("$(wildcard /.dockerenv)", "")
	docker_run =
endif

# Default config file path (starting at git root)
# When running ns-3
app_config ?= rdma-config/config.json

# Build type when configuring CMake
build_type ?= default

.PHONY: build_image
build_image:
	docker build -t $(docker_tag) .

ns3_run = ./simulation/ns3 run 'rdma ../$(app_config)'

.PHONY: configure_debug
configure_debug: build_type = debug
configure_debug: configure

.PHONY: configure_release
configure_release: build_type = optimized
configure_release: configure

.PHONY: configure
configure:	
	$(docker_run) ./simulation/ns3 configure -d $(build_type) --disable-werror --cxx-standard=20

.PHONY: build
build:
	$(docker_run) ./simulation/ns3 build rdma

# Clean all build files + generated binaries
.PHONY: distclean
distclean:
	$(docker_run) ./simulation/ns3 clean

.PHONY: run
run:
	$(docker_run) $(ns3_run)

.PHONY: run_gdb
run_gdb:
	$(docker_run) $(ns3_run) --command-template="gdb -ex run --args %s"

.PHONY: run_valgrind
run_valgrind:
	$(docker_run) $(ns3_run) --command-template="valgrind %s "

.PHONY: run_bash
run_bash: docker_user =
run_bash:
	$(docker_run) /bin/bash

.PHONY: gen_avro
gen_avro:
	$(docker_run) ./simulation/src/rdma/serdes/schemas/gen-avro-headers.sh

#################
################# Analysis
#################

.PHONY: analysis
analysis: docker_wd = -w /app/analysis/src
analysis:
	$(docker_run) python3 -m models.ft16

#################
################# Netanim
#################

.PHONY: build_netanim
build_netanim:
	$(docker_run) bash build-netanim.sh

.PHONY: run_netanim
run_netanim: docker_env     += -e DISPLAY=$(DISPLAY)
run_netanim: docker_extra   += -v /tmp/.X11-unix:/tmp/.X11-unix
run_netanim:
	$(docker_run) ./netanim/NetAnim

#################
################# Trace Reader
#################

build_trace: 
	$(docker_run) make -C analysis