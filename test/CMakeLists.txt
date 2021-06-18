cmake_minimum_required(VERSION 3.2)
project(bus_test)

get_target_property(TEST_SOFTBUS_INCLUDES softbus INCLUDE_DIRECTORIES)

function(BUS_ADD_TEST name)
	add_executable(${name} ${name}.c)
	target_link_libraries(${name} PRIVATE softbus)
	target_include_directories(${name}
		PRIVATE
			${TEST_SOFTBUS_INCLUDES}
			${CMAKE_CURRENT_SOURCE_DIR})
	add_test(${name} ${name})
endfunction()

BUS_ADD_TEST(call_in_stack)
