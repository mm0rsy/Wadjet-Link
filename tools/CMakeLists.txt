# ============================================================================
# Wadjet CLI Tools
# ============================================================================

# T128: wadjet-coordinator - Distributed test coordinator CLI (T121-T124)
add_executable(wadjet-coordinator
    wadjet-coordinator.cpp
)

target_link_libraries(wadjet-coordinator
    PRIVATE
        wadjet
        wadjet_distributed
        wadjet_warnings
        wadjet_sanitizers
)

install(TARGETS wadjet-coordinator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# T128: wadjet-node - Distributed test node CLI (T125-T127)
add_executable(wadjet-node
    wadjet-node.cpp
)

target_link_libraries(wadjet-node
    PRIVATE
        wadjet
        wadjet_distributed
        wadjet_warnings
        wadjet_sanitizers
)

install(TARGETS wadjet-node
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# wadjet-run: Scenario runner CLI with optional distributed support
if(WADJET_SCENARIO_ENABLED)
    add_executable(wadjet-run
        wadjet-run.cpp
    )

    target_link_libraries(wadjet-run
        PRIVATE
            wadjet
            wadjet_warnings
            wadjet_sanitizers
    )
    
    # T329: Link distributed coordinator when distributed testing enabled
    if(WADJET_ENABLE_DISTRIBUTED)
        target_link_libraries(wadjet-run
            PRIVATE
                wadjet_distributed
        )
        target_compile_definitions(wadjet-run
            PRIVATE
                WADJET_ENABLE_DISTRIBUTED
        )
    endif()

    install(TARGETS wadjet-run
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
else()
    message(STATUS "wadjet-run: DISABLED (requires scenario module)")
endif()
