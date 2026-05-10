cmake_minimum_required(VERSION 3.14)

if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
    "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(SoapyIcom7610 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# SoapySDR (installiert von PothosSDR nach C:\Program Files\PothosSDR)
# ---------------------------------------------------------------------------
find_package(SoapySDR REQUIRED)

# ---------------------------------------------------------------------------
# Plugin als Shared Library (.dll) — kein Executable!
# ---------------------------------------------------------------------------
add_library(SoapyIcom7610 SHARED
    SoapyIcom7610.cpp
    SoapyIcom7610.h
)

# ---------------------------------------------------------------------------
# FTD3XX — Dateien direkt im Projektverzeichnis
# ${CMAKE_CURRENT_SOURCE_DIR} = E:\Icom7610\SoapyIcom7610\
# Benötigte Dateien aus FTD3XXLibrary_v1.3.0.2.zip:
#   FTD3XX.h   → aus \inc\
#   FTD3XX.lib → aus \lib\cpp\x64\  (für x64 Release/Debug)
# ---------------------------------------------------------------------------
target_include_directories(SoapyIcom7610 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_directories(SoapyIcom7610 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SoapyIcom7610 PRIVATE
    SoapySDR
    FTD3XX
)

# ---------------------------------------------------------------------------
# SoapySDR Plugin-Installationspfad
# Nach Build: SoapySDRUtil --find sollte "IC7610" anzeigen
# ---------------------------------------------------------------------------
install(TARGETS SoapyIcom7610
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/SoapySDR/modules${SoapySDR_ABI_VERSION}
)
