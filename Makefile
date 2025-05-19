# All commands are run in the docker container,
# so no dependencies except Make and Docker.
#
#
#

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
app_config ?= rdma-config/default-config.json

# Build type when configuring CMake
build_type ?= default

.PHONY: build_image
build_image:
	docker build -t $(docker_tag) .

ns3_run = ./simulation/ns3 run 'scratch_rdma-ag ../$(app_config)'

# `NS3_OUTPUT_DIRECTORY` should be child of `./simulation`.
# Then, build directory should also be child of `./simulation` to not pollute too much.
ns3_build_dir = cmake-build-$(cmake_build_name)
cmake_vars = -DNS3_WARNINGS_AS_ERRORS=OFF \
	-DCMAKE_BUILD_TYPE='$(cmake_build_type)' \
	-DCMAKE_CXX_STANDARD=20 \
	-DNS3_OUTPUT_DIRECTORY=$(ns3_build_dir)
cmake_build_dir = simulation/$(ns3_build_dir)/cmake-cache

.PHONY: configure_debug
configure_debug: cmake_build_type = Debug
configure_debug: cmake_build_name = debug
configure_debug: cmake_vars += -DNS3_ASSERT=ON -DNS3_LOG=ON -DNS3_NATIVE_OPTIMIZATIONS=OFF
configure_debug: configure

.PHONY: configure_release
configure_release: cmake_build_type = Release
configure_release: cmake_build_name = release
configure_release: cmake_vars += -DNS3_ASSERT=OFF -DNS3_LOG=OFF -DNS3_NATIVE_OPTIMIZATIONS=ON
configure_release: configure

# Define `NS3_OUTPUT_DIRECTORY` so can co-exist Debug and Release builds
# instead of sharing `simulation/build` directory.
.PHONY: configure
configure:
	$(docker_run) cmake \
		-S ./simulation \
		-B '$(cmake_build_dir)' \
		-G Ninja \
		$(cmake_vars)

.PHONY: build_debug
build_debug: cmake_build_name = debug
build_debug: build

.PHONY: build_release
build_release: cmake_build_name = release
build_release: build

.PHONY: build
build:
	$(docker_run) cmake \
		--build '$(cmake_build_dir)' \
		--target scratch_rdma-ag

# Clean all build files + generated binaries
.PHONY: distclean
distclean:
	$(docker_run) ./simulation/ns3 clean

.PHONY: run_debug
run_debug: scratch_exe = ./simulation/$(ns3_build_dir)/scratch/ns3.36.1-scratch_rdma-ag-debug
run_debug: cmake_build_name = debug
run_debug: run

.PHONY: run_release
run_release: scratch_exe = ./simulation/$(ns3_build_dir)/scratch/ns3.36.1-rdma-ag-optimized
run_release: cmake_build_name = release
run_release: run

.PHONY: run
run:
	$(docker_run) '$(scratch_exe)' '$(app_config)'


.PHONY: run_gdb
run_gdb:
	$(docker_run) $(ns3_run) --command-template="gdb -ex run --args %s"

.PHONY: run_valgrind
run_valgrind:
	$(docker_run) $(ns3_run) --command-template="valgrind %s "

.PHONY: run_massif
run_massif:
	$(docker_run) $(ns3_run) --command-template="valgrind --tool=massif %s "

.PHONY: run_bash
run_bash: docker_user =
run_bash:
	$(docker_run) /bin/bash

.PHONY: gen_avro
gen_avro:
	$(docker_run) ./simulation/src/rdma-core/serdes/scripts/gen-avro-headers.sh

#################
################# Analysis
#################

.PHONY: analysis
analysis: docker_wd = -w /app/analysis/src
analysis: analysis_model ?= bitmap-repartition
analysis:
	$(docker_run) bash -c "cd analysis/src && python3 -m models.$(analysis_model)"

.PHONY: plots
plots: docker_wd = -w /app/analysis/src
plots:
	$(docker_run) python3 -m pr.efficiency

#################
################# Netanim
#################

.PHONY: build_netanim
build_netanim:
	$(docker_run) bash netanim/build-netanim.sh

.PHONY: run_netanim
run_netanim: docker_env     += -e DISPLAY=$(DISPLAY)
run_netanim: docker_extra   += -v /tmp/.X11-unix:/tmp/.X11-unix
run_netanim:
	$(docker_run) ./netanim/run-netanim.sh

#################
################# Trace Reader
#################

build_trace: 
	$(docker_run) make -C analysis