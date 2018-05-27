cmake_minimum_required(VERSION 3.0.0)

Project(MqttRPC)

function(assign_source_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else()
            set(source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_source_group)

set(MQTTRPC_SOURCES

	${CMAKE_CURRENT_SOURCE_DIR}/Source/Mqtt.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Source/Rpc.cpp
)

set(MQTTRPC_INCLUDES

	${CMAKE_CURRENT_SOURCE_DIR}/Include/Mqtt.h
	${CMAKE_CURRENT_SOURCE_DIR}/Include/Rpc.h
	${CMAKE_CURRENT_SOURCE_DIR}/Include/Shared.h
)

assign_source_group(${MQTTRPC_SOURCES})
assign_source_group(${MQTTRPC_INCLUDES})

add_library(MqttRPC STATIC ${MQTTRPC_SOURCES} ${MQTTRPC_INCLUDES})
set_target_properties(MqttRPC PROPERTIES LINKER_LANGUAGE CXX)


option(WITH_TLS OFF)
option(WITH_DNS_SRV OFF)
option(WITH_BROKER OFF)
option(WITH_THREADING OFF)
set(WITH_STATIC_LIBRARIES ON CACHE BOOL "Build Static libraries")
add_definitions(-Dmosquittopp_EXPORTS)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/mosquitto)


add_executable(SimpleExample ${CMAKE_CURRENT_SOURCE_DIR}/Examples/Simple.cpp)
target_link_libraries(SimpleExample MqttRPC mosquittopp_static libmosquitto_static )

if (WIN32)
	
    set_target_properties(MqttRPC PROPERTIES COMPILE_FLAGS "/std:c++latest ")
	set_target_properties(SimpleExample PROPERTIES COMPILE_FLAGS "/std:c++latest ")
else ()
    set_target_properties(MqttRPC PROPERTIES COMPILE_FLAGS "-std=gnu++1z -fpermissive " )
	set_target_properties(SimpleExample PROPERTIES COMPILE_FLAGS "-std=gnu++1z -fpermissive " )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/External/cereal/include)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/External/mosquitto/lib/cpp)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/External/mosquitto/lib/)

