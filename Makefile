TAG = hpcc
DOCKER_USER = -u $(shell id -u):$(shell id -g) # Avoid creating files as root
DOCKER_MOUNT = --mount type=bind,src=.,dst=/app
DOCKER_RUN = docker run --rm -it $(DOCKER_MOUNT) $(DOCKER_USER) -w /app/simulation -e NS_LOG=$(NS_LOG) $(TAG)
.PHONY: build_image configure build run build_trace

# config file from inside the container path when running ns-3
config ?= mix/my/myconfig.txt

build_image:
	docker build -t $(TAG) .

# Configure waf
configure_debug:
	$(DOCKER_RUN) python waf configure -d debug

configure_release:
	$(DOCKER_RUN) python waf configure -d release

# Build waf
build:
	$(DOCKER_RUN) python waf build

# Run HPCC
# Working directory inside the Docker container is "$GIT_ROOT/simulation".
run:
	$(DOCKER_RUN) python waf --run 'scratch/third $(config)'

run_gdb:
	$(DOCKER_RUN) python waf --run 'scratch/third' --command-template="gdb -ex run --args %s $(config)"

# Build trace reader
build_trace: 
	$(DOCKER_RUN) make -C ../analysis