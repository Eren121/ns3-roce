# Rdma Allgather implementation

build_lib(
  LIBNAME rdma-ag
  SOURCE_FILES
    ag-app.cc
    ag-app-helper.cc
    ag-config.cc
    ag-runtime.cc
    ag-shared.cc
    ag-app.cc
    ag-app-helper.cc
    ag-config.cc
    ag-runtime.cc
    ag-shared.cc 
    ag-flow-mcast-phase.cc 
  HEADER_FILES
    ag-app.h
    ag-app-helper.h
    ag-config.h
    ag-runtime.h
    ag-shared.h
    ag-flow-mcast-phase.h
  LIBRARIES_TO_LINK
    ${librdma-core}
)