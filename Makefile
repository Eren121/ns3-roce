TAG = hpcc
DOCKER_USER = -u $(shell id -u):$(shell id -g) # Avoid creating files as root
DOCKER_MOUNT = --mount type=bind,src=.,dst=/app
DOCKER_RUN = docker run --rm $(DOCKER_MOUNT) $(DOCKER_USER) -w /app/simulation -e NS_LOG=$(NS_LOG) $(TAG)
.PHONY: build_image configure build run build_trace

build_image:
	docker build -t $(TAG) .

# Configure waf
configure:
	$(DOCKER_RUN) python waf configure

# Build waf
build:
	$(DOCKER_RUN) python waf build

# Run HPCC
run:
	$(DOCKER_RUN) python waf --run 'scratch/third mix/my/myconfig.txt'

# Build trace reader
build_trace: 
	$(DOCKER_RUN) make -C ../analysis