# ============================================================================
# Wadjet CLI Tools
# ============================================================================

# wadjet-run: Scenario runner CLI
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

    install(TARGETS wadjet-run
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
else()
    message(STATUS "wadjet-run: DISABLED (requires scenario module)")
endif()
