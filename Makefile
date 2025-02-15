TAG = hpcc
DOCKER_USER ?= -u $(shell id -u):$(shell id -g) # Avoid creating files as root
DOCKER_MOUNT ?= --mount type=bind,src=.,dst=/app
DOCKER_RUN ?= docker run --rm $(docker_interactive) $(DOCKER_MOUNT) $(DOCKER_USER) -w /app/simulation -e NS_LOG='$(NS_LOG)' -e CXXFLAGS='-Wall -fdiagnostics-color=always' -e LD_LIBRARY_PATH=build $(TAG)
.PHONY: build_image configure build run build_trace

# config file from inside the container path when running ns-3
config ?= mix/my/myconfig.json

docker_interactive ?= -it

build_image:
	docker build -t $(TAG) .

# Configure waf
configure_debug:
	 $(DOCKER_RUN) python2 waf configure -d debug

configure_release:
	 $(DOCKER_RUN) python2 waf configure -d release

# Build waf
build:
	$(DOCKER_RUN) python2 waf build

# Build netanim
build_netanim:
	$(DOCKER_RUN) bash ../build-netanim.sh

run_netanim:
	docker run --rm -it $(DOCKER_MOUNT) $(DOCKER_USER) \
	 -e DISPLAY=$(DISPLAY) \
	 -w /app/netanim -v /tmp/.X11-unix:/tmp/.X11-unix \
	 $(TAG) ./NetAnim

# Clean all build files + generated binaries
distclean:
	$(DOCKER_RUN) python2 waf distclean

# Run HPCC
# Working directory inside the Docker container is "$GIT_ROOT/simulation".
# Don't use waf because running waf in parallel doeesnt work very well (some clang's waf's files are modified)
run:
	$(DOCKER_RUN) build/scratch/third $(config)

run_gdb:
	$(DOCKER_RUN) python2 waf --run 'scratch/third' --command-template="gdb -ex run --args %s $(config)"

run_bash: DOCKER_USER=
run_bash:
	$(DOCKER_RUN) /bin/bash

# Build trace reader
build_trace: 
	$(DOCKER_RUN) make -C ../analysis