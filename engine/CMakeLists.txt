# The COMPLICATOR audio engine. Plain C++17, no JUCE, no plugin code.
# Later plugins can reuse this folder (oscillators, band-limiting, decimator, pitch rules).

add_library(complicator_engine STATIC
    src/Engine.cpp
    src/Oscillators.cpp
    src/Decimator.cpp
)

target_include_directories(complicator_engine PUBLIC include)
target_compile_features(complicator_engine PUBLIC cxx_std_17)

if(COMPLICATOR_BUILD_TESTS)
    add_executable(complicator_engine_test tests/engine_test.cpp)
    target_link_libraries(complicator_engine_test PRIVATE complicator_engine)
    add_test(NAME engine COMMAND complicator_engine_test)
endif()
