# ns3-roce

RoCE simulation in ns-3.
Based on HPCC, which is based on ns3-coneweave, wich is based on ns3-rdma.

New features:
- Add support for RDMA unreliable datagram (UD).
- Add support for multicast.

## Build

Refer to the `Makefile` for building the project.
1. Build docker image: `make build_image`
1. Run example: `make configure_release && make build && make run`
3. Build netanim: `make build_netanim`. Output is `netanim/NetAnim`.

## Changes

1. Updated to a more recent version of ns3.
1. Fix some bugs.
1. Refactor code.
1. Add netanim support.
1. Add Docker image.

The original project is still on ns-3.16, which causes problems on recent compilers: Now, a `gcc` version greather than 6 is working properly. This version upgrades the code to ns-3.26. Upgrading the version permits also to generate XML traces compatible with netanim, as the original version was producing traces with netanim-3.101, which we did not found online.

Some bugs were fixed:

- ACK interval is not ignored anymore.
- Disabling ACK interval with (AckInterval=0) is now working properly.
- Traffic is now properly seen in netanim XML trace. 
- Other minor bugs.

Refactoring:

- Now all new files are grouped in a `rdma` module, under `simulation/rdma` folder, which makes more easy to identify the changes.
- Remove most of the changes of the original project to ns3 source code to make more easy to upgrade to newer versions of ns3.
- Configuration is now in Json.

## NS-3 simulation
The ns-3 simulation is under `simulation/`. Refer to the README.md under it for more details.

## Traffic generator
The traffic generator is under `traffic_gen/`. Refer to the README.md under it for more details.

## Analysis
We provide a few analysis scripts under `analysis/` to view the packet-level events, and analyzing the fct in the same way as [HPCC](https://liyuliang001.github.io/publications/hpcc.pdf) Figure 11.
Refer to the README.md under it for more details.

## Original authors

HPCC:

- Rui Miao (miao.rui@alibaba-inc.com).
