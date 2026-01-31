find_package(Qt6 REQUIRED COMPONENTS Test)

# Helper function to add a QTest-based test
function(add_qtest TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE game-rewind-lib Qt6::Test)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

add_qtest(test_steamutils test_steamutils.cpp)
add_qtest(test_database test_database.cpp)
add_qtest(test_savemanager test_savemanager.cpp)
add_qtest(test_profiledetector test_profiledetector.cpp)
