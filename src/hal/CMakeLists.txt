add_library(hal INTERFACE)
target_include_directories(hal INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(hal INTERFACE xmotion::xmSigma)   # for xmotion/types/*
add_library(xmotion::hal ALIAS hal)
install(TARGETS hal EXPORT xmMuTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
