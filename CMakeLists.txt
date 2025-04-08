# CMakeList.txt : CMake project for thread-bypass, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (thread-bypass)

# Set our C++ standard
set(CMAKE_CXX_STANDARD 23)

#set(CMAKE_BUILD_TYPE RelWithDebInfo)

# Include fetch content
include(FetchContent)

# Fetch the latest version of wincpp
FetchContent_Declare (
	wincpp 
	URL https://github.com/atrexus/wincpp/releases/download/v1.5.3.5/wincpp-src.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable (wincpp)

# Set the source files
set(SRC
	"src/main.cpp"
)

# Add source to this project's executable.
add_executable (thread-bypass ${SRC} ${HDR})

# Set the include directories
target_include_directories(thread-bypass PRIVATE "include")

# Link the libraries
target_link_libraries(thread-bypass PRIVATE wincpp)