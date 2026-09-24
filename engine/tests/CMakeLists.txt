# Test infrastructure
# Basic unit tests
add_executable(test_hash_table test_hash_table.cpp)
target_link_libraries(test_hash_table engine)
target_include_directories(test_hash_table PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_lru test_lru.cpp)
target_link_libraries(test_lru engine)
target_include_directories(test_lru PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_lfu test_lfu.cpp)
target_link_libraries(test_lfu engine)
target_include_directories(test_lfu PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_ttl test_ttl.cpp)
target_link_libraries(test_ttl engine)
target_include_directories(test_ttl PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_sharded_engine test_sharded_engine.cpp)
target_link_libraries(test_sharded_engine engine pthread)
target_include_directories(test_sharded_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_wal test_wal.cpp)
target_link_libraries(test_wal engine)
target_include_directories(test_wal PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_snapshot test_snapshot.cpp)
target_link_libraries(test_snapshot engine)
target_include_directories(test_snapshot PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(test_resp test_resp.cpp)
target_link_libraries(test_resp engine)
target_include_directories(test_resp PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME hash_table_unit_tests COMMAND test_hash_table)
add_test(NAME lru_eviction_tests COMMAND test_lru)
add_test(NAME lfu_eviction_tests COMMAND test_lfu)
add_test(NAME ttl_expiration_tests COMMAND test_ttl)
add_test(NAME sharded_engine_tests COMMAND test_sharded_engine)
add_test(NAME wal_tests COMMAND test_wal)
add_test(NAME snapshot_tests COMMAND test_snapshot)
add_test(NAME resp_protocol_tests COMMAND test_resp)

# Future enhancement: Property-based testing with RapidCheck
# Potential properties to validate:
# - Property 1: SET-GET round trip
# - Property 2: DELETE removes keys  
# - Property 3: Non-existent keys return null
# - Property 4: Hash collisions preserve data
