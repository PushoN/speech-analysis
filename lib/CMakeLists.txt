cmake_minimum_required(VERSION 3.0)
project(libspeech)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-fPIC -mtune=generic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

set(SOURCES
    Formant/Formant.cpp
    Formant/Formant.h
    Formant/track.cpp
    Formant/EKF/EKF.h
    Formant/EKF/fb2cp.cpp
    Formant/EKF/getH_FBW.cpp
    Formant/EKF/genLPCC.cpp
    Formant/EKF/init.cpp
    Formant/EKF/step.cpp
    LPC/Frame/LPC_Frame.cpp
    LPC/Frame/LPC_Frame.h
    LPC/Frame/LPC_Frame_auto.cpp
    LPC/Frame/LPC_Frame_burg.cpp
    LPC/Frame/LPC_Frame_covar.cpp
    LPC/Frame/LPC_Frame_huber.cpp
    LPC/LPC.cpp
    LPC/LPC.h
    LPC/LPC_filter.cpp
    LPC/LPC_huber.cpp
    LPC/LPC_huber.h
    LPC/LPC_huber_stat.cpp
    LPC/residual.cpp
    Math/Bairstow.cpp
    Math/Bairstow.h
    Math/Polynomial.cpp
    Math/Polynomial.h
    Math/Viterbi.cpp
    Math/Viterbi.h
    Pitch/McLeod/autocorrelation.cpp
    Pitch/McLeod/parabolic_interpolation.cpp
    Pitch/McLeod/peak_picking.cpp
    Pitch/McLeod/MPM.h
    Pitch/Yin/autocorrelation.cpp
    Pitch/Yin/parabolic_interpolation.cpp
    Pitch/Yin/difference.cpp
    Pitch/Yin/cmnd.cpp
    Pitch/Yin/threshold.cpp
    Pitch/Pitch_AMDF.cpp
    Pitch/Pitch_MPM.cpp
    Pitch/Pitch_DynWav.cpp
    Pitch/Pitch_YIN.cpp
    Pitch/Pitch.h
    GCOI/GCOI.h
    GCOI/findpeaks.cpp
    GCOI/SEDREAMS.cpp
    GCOI/MP.cpp
    GCOI/OQ.cpp
    GCOI/gci_sedreams/gci_sedreams.h
    GCOI/gci_sedreams/sedreams.cpp
    GCOI/gci_sedreams/zerocrossings.cpp
    GCOI/gci_sedreams/findpeaks.cpp
    GCOI/gci_sedreams/median.cpp
    Signal/Filter.cpp
    Signal/Filter.h
    Signal/Resample.cpp
    Signal/Resample.h
    Signal/Window.cpp
    Signal/Window.h
    FFT/FFT.cpp
    FFT/FFT.h
    MFCC/MFCC.cpp
    MFCC/MFCC.h)

find_package(Eigen3 REQUIRED NO_MODULE)

if (FFTW_ROOT)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endif()

find_package(FFTW REQUIRED COMPONENTS DOUBLE_LIB)

if (APPLE)
    include_directories($ENV{OSXCROSS}/SDK/MacOSX10.13.sdk/usr/include/c++/v1)
endif()

include_directories(
    ${EIGEN_INCLUDE_DIRS}
    ${FFTW_INCLUDE_DIRS}
)

if (ANDROID)
    add_library(speech SHARED ${SOURCES})
else()
    add_library(speech STATIC ${SOURCES})
endif()

target_link_libraries(speech
    Eigen3::Eigen
    ${FFTW_LIBRARIES}
)

