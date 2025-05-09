find_package(reflectcpp REQUIRED)
find_library(libavrocpp avrocpp REQUIRED)

if(reflectcpp_FOUND)
  build_lib(
    LIBNAME rdma
    SOURCE_FILES
      ag/ag-app.cc
      ag/ag-app-helper.cc
      ag/ag-config.cc
      ag/ag-runtime.cc
      ag/ag-shared.cc
      app/rdma-flow.cc
      app/rdma-network.cc
      app/rdma-qlen-monitor.cc
      app/rdma-unicast-app.cc
      app/rdma-unicast-app-helper.cc
      helper/rdma-helper.cc
      helper/rdma-reflection-helper.cc
      model/broadcom-egress-queue.cc
      model/cn-header.cc
      model/custom-header.cc
      model/data-rate-ops.cc
      model/int-header.cc
      model/pause-header.cc
      model/pint.cc
      model/qbb-channel.cc
      model/qbb-header.cc
      model/qbb-helper.cc
      model/qbb-net-device.cc
      model/qbb-remote-channel.cc
      model/rdma-bth.cc
      model/rdma-hw.cc
      model/rdma-queue-pair.cc
      model/rdma-random.cc
      model/rdma-reliable-qp.cc
      model/rdma-seq-header.cc
      model/rdma-unreliable-qp.cc
      model/switch-mmu.cc
      model/switch-node.cc
      ag/ag-app.cc
      ag/ag-app-helper.cc
      ag/ag-config.cc
      ag/ag-runtime.cc
      ag/ag-shared.cc
      app/rdma-flow.cc
      app/rdma-network.cc
      app/rdma-qlen-monitor.cc
      app/rdma-qp-monitor.cc
      app/rdma-unicast-app.cc
      app/rdma-unicast-app-helper.cc
      helper/rdma-helper.cc
      helper/rdma-reflection-helper.cc
      model/broadcom-egress-queue.cc
      model/cn-header.cc
      model/custom-header.cc
      model/data-rate-ops.cc
      model/int-header.cc
      model/pause-header.cc
      model/pint.cc
      model/qbb-channel.cc
      model/qbb-header.cc
      model/qbb-helper.cc
      model/qbb-net-device.cc
      model/qbb-remote-channel.cc
      model/rdma-bth.cc
      model/rdma-hw.cc
      model/rdma-queue-pair.cc
      model/rdma-random.cc
      model/rdma-reliable-qp.cc
      model/rdma-seq-header.cc
      model/rdma-unreliable-qp.cc
      model/switch-mmu.cc
      model/switch-node.cc        
    HEADER_FILES
      serdes/ag-recv-chunk-record.h
      serdes/rdma-serdes.h
      ag/ag-app.h
      ag/ag-app-helper.h
      ag/ag-config.h
      ag/ag-runtime.h
      ag/ag-shared.h
      app/rdma-flow.h
      app/rdma-network.h
      app/rdma-qlen-monitor.h
      app/rdma-qp-monitor.h
      app/rdma-unicast-app.h
      app/rdma-unicast-app-helper.h
      app/sim-setting.h
      helper/filesystem.h
      helper/json.h
      helper/rdma-helper.h
      helper/rdma-reflection-helper.h
      model/broadcom-egress-queue.h
      model/cn-header.h
      model/custom-header.h
      model/data-rate-ops.h
      model/int-header.h
      model/pause-header.h
      model/pint.h
      model/qbb-channel.h
      model/qbb-header.h
      model/qbb-helper.h
      model/qbb-net-device.h
      model/qbb-remote-channel.h
      model/rdma-bth.h
      model/rdma.h
      model/rdma-hw.h
      model/rdma-queue-pair.h
      model/rdma-random.h
      model/rdma-reliable-qp.h
      model/rdma-seq-header.h
      model/rdma-unreliable-qp.h
      model/switch-mmu.h
      model/switch-node.h
      model/trace-format.h
    LIBRARIES_TO_LINK reflectcpp ${libavrocpp}
                      ${libinternet}
                      ${libnetwork}
                      ${libpoint-to-point}
                      ${libapplications}
  )
endif()