# Well shit don't blame me, blame Live2D Cubism Native Core uses CMake 3.6
cmake_minimum_required (VERSION 3.6)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

###############
# Some checks #
###############

# Prevent in-tree build.
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	message(FATAL_ERROR "Prevented in-tree build!")
endif()

# Check Live2D source/header files
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/include/Live2DCubismCore.h")
	message(FATAL_ERROR "Live2D Cubism 3 SDK for Native is missing!")
endif()

#################
# Project stuff #
#################

project(Live2LOVE LANGUAGES C CXX)

if(MSVC)
	option(LIVE2LOVE_MT "Build multi-thread (/MT) version of library" OFF)
endif()

# Cmake in core does not exist for sdk4+
# add_subdirectory("live2d/Core")

# Refer to live2d framework sample
add_library(Live2DCubismCore STATIC IMPORTED)
set(CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core)
# Get architecture.
EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
# Set library path and inlude path.
set_target_properties(Live2DCubismCore
  PROPERTIES
    IMPORTED_LOCATION ${CORE_PATH}/lib/macos/${ARCHITECTURE}/libLive2DCubismCore.a
    INTERFACE_INCLUDE_DIRECTORIES ${CORE_PATH}/include
)

# Live2LOVE sources
set(LIVE2LOVE_SOURCE_FILES
	src/Live2LOVE.cpp
	src/RefData.cpp
	src/Main.cpp
)

# Live2D Cubism 3 Framework files, copied straight from `live2d/Framework/CMakeLists.txt`
set(LIVE2D_CUBISM3_FRAMEWORK
    live2d/Framework/src/Effect/CubismBreath.cpp
    live2d/Framework/src/Effect/CubismEyeBlink.cpp
    live2d/Framework/src/Effect/CubismPose.cpp

    live2d/Framework/src/Id/CubismId.cpp
    live2d/Framework/src/Id/CubismIdManager.cpp

    live2d/Framework/src/Math/CubismMath.cpp
    live2d/Framework/src/Math/CubismMatrix44.cpp
    live2d/Framework/src/Math/CubismModelMatrix.cpp
    live2d/Framework/src/Math/CubismTargetPoint.cpp
    live2d/Framework/src/Math/CubismVector2.cpp
    live2d/Framework/src/Math/CubismViewMatrix.cpp

    live2d/Framework/src/Model/CubismModel.cpp
    live2d/Framework/src/Model/CubismModelUserData.cpp
    live2d/Framework/src/Model/CubismModelUserDataJson.cpp
    live2d/Framework/src/Model/CubismMoc.cpp

    live2d/Framework/src/Motion/CubismExpressionMotion.cpp
    live2d/Framework/src/Motion/CubismMotion.cpp
    live2d/Framework/src/Motion/CubismMotionJson.cpp
    live2d/Framework/src/Motion/CubismMotionManager.cpp
    live2d/Framework/src/Motion/CubismMotionQueueEntry.cpp
    live2d/Framework/src/Motion/CubismMotionQueueManager.cpp
    live2d/Framework/src/Motion/ACubismMotion.cpp

    live2d/Framework/src/Physics/CubismPhysicsJson.cpp
    live2d/Framework/src/Physics/CubismPhysics.cpp

    live2d/Framework/src/Rendering/CubismRenderer.cpp

    live2d/Framework/src/Type/csmRectF.cpp
    live2d/Framework/src/Type/csmString.cpp

    live2d/Framework/src/Utils/CubismDebug.cpp
    live2d/Framework/src/Utils/CubismJson.cpp
    live2d/Framework/src/Utils/CubismString.cpp

    live2d/Framework/src/CubismDefaultParameterId.cpp
    live2d/Framework/src/CubismFramework.cpp
    live2d/Framework/src/CubismModelSettingJson.cpp
)
source_group(Live2DFramework FILES ${LIVE2D_CUBISM3_FRAMEWORK})

if(BUILD_SHARED_LIBS)
	add_library(Live2LOVE SHARED ${LIVE2LOVE_SOURCE_FILES} ${LIVE2D_CUBISM3_FRAMEWORK})
else()
	add_library(Live2LOVE STATIC ${LIVE2LOVE_SOURCE_FILES} ${LIVE2D_CUBISM3_FRAMEWORK})
endif()

set_target_properties(Live2LOVE PROPERTIES POSITION_INDEPENDENT_CODE ON)

# According to Core CMakeLists.txt, this shouldn't be "OFF" if there are other deps
if(NOT ${CSM_CORE_DEPS} STREQUAL "OFF")
	add_dependencies(Live2LOVE ${CSM_CORE_DEPS})
endif()

# MSVC-specific.
# MSVC is somewhat messy because we must account for multiple types
if(MSVC)
	target_compile_definitions(Live2LOVE PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE LUA_BUILD_AS_DLL LUA_LIB)

	# Select correct MSVC version
	if((${MSVC_VERSION} EQUAL 1900) OR (${MSVC_VERSION} GREATER 1900))
		set(_LIVE2LOVE_MSVC_LINK 140)
	else()
		set(_LIVE2LOVE_MSVC_LINK 120)
	endif()

	# Is it 64-bit build?
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(_LIVE2LOVE_WINARCH x86_64)
		target_link_libraries(Live2LOVE ${CMAKE_CURRENT_SOURCE_DIR}/lib/Win32/lua51_x64.lib)
	else()
		set(_LIVE2LOVE_WINARCH x86)
		target_link_libraries(Live2LOVE ${CMAKE_CURRENT_SOURCE_DIR}/lib/Win32/lua51.lib)
	endif()

	# Are we building with MT switch?
	if(LIVE2LOVE_MT)
		set(_LIVE2LOVE_CRT_TYPE MT)
	else()
		set(_LIVE2LOVE_CRT_TYPE MD)
	endif()

	set(_LIVE2LOVE_RELEASE_OPTION "-${_LIVE2LOVE_CRT_TYPE}")
	set(_LIVE2LOVE_DEBUG_OPTION "-${_LIVE2LOVE_CRT_TYPE}d")

	target_compile_options(Live2LOVE PUBLIC "$<$<CONFIG:DEBUG>:${_LIVE2LOVE_DEBUG_OPTION}>")
	target_compile_options(Live2LOVE PUBLIC "$<$<CONFIG:RELEASE>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_compile_options(Live2LOVE PUBLIC "$<$<CONFIG:RELWITHDEBINFO>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_compile_options(Live2LOVE PUBLIC "$<$<CONFIG:MINSIZEREL>:${_LIVE2LOVE_RELEASE_OPTION}>")
	target_link_libraries(Live2LOVE
		debug ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/lib/windows/${_LIVE2LOVE_WINARCH}/${_LIVE2LOVE_MSVC_LINK}/Live2DCubismCore_${_LIVE2LOVE_CRT_TYPE}d.lib
		optimized ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/lib/windows/${_LIVE2LOVE_WINARCH}/${_LIVE2LOVE_MSVC_LINK}/Live2DCubismCore_${_LIVE2LOVE_CRT_TYPE}.lib
	)
	message(STATUS "Selected Live2D Core MSVC ver: ${_LIVE2LOVE_MSVC_LINK} (${_LIVE2LOVE_WINARCH})")
else()
	find_package(Lua 5.1 EXACT REQUIRED)
	
	if(UNIX AND NOT RPI AND NOT ANDROID AND NOT APPLE)
		# Unfortunately libLive2DCubismCore.a (provided by ${CSM_CORE_LIBS}) is not compiled with fPIC
		# so we can't link with it.
		target_include_directories(Live2LOVE PRIVATE ${LUA_INCLUDE_DIR})
		target_link_libraries(Live2LOVE ${LUA_LIBRARIES} ${CMAKE_CURRENT_SOURCE_DIR}/live2d/Core/dll/linux/x86_64/libLive2DCubismCore.so)
	else()
		# todo properly branch for mac
		message("MAC")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup")
		target_link_libraries(Live2LOVE Live2DCubismCore)
	endif()
endif()

target_include_directories(Live2LOVE PRIVATE include live2d/Framework/src live2d/Core/include)
install(TARGETS Live2LOVE DESTINATION lib)
