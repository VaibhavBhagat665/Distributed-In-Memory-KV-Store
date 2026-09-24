# Benchmark infrastructure
# Benchmark executables for measuring storage engine performance

add_executable(bench_single_threaded bench_single_threaded.cpp)
target_link_libraries(bench_single_threaded engine)
target_include_directories(bench_single_threaded PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(bench_multi_threaded bench_multi_threaded.cpp)
target_link_libraries(bench_multi_threaded engine pthread)
target_include_directories(bench_multi_threaded PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(bench_comparison bench_comparison.cpp)
target_link_libraries(bench_comparison engine pthread)
target_include_directories(bench_comparison PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(bench_persistence bench_persistence.cpp)
target_link_libraries(bench_persistence engine)
target_include_directories(bench_persistence PRIVATE ${CMAKE_SOURCE_DIR}/include)
