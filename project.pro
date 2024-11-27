QT += core gui widgets

CONFIG += c++11
TEMPLATE = app
TARGET = shared_memory_viewer

# OpenCV Configuration
OPENCV_PATH = /usr/local/opencv
INCLUDEPATH += $$OPENCV_PATH/include/opencv4 \
                /usr/include/opencv4 \
                /usr/include/x86_64-linux-gnu/qt5 \
                /usr/include/x86_64-linux-gnu/qt5/QtWidgets \
                /usr/include/x86_64-linux-gnu/qt5/QtGui \
                /usr/include/x86_64-linux-gnu/qt5/QtCore \
                /usr/include/c++/11 \
                /usr/include/x86_64-linux-gnu/c++/11 \
                /usr/include/c++/11/backward \
                /usr/include/boost

LIBS += -L$$OPENCV_PATH/lib \
        -lopencv_core \
        -lopencv_imgproc \
        -lopencv_highgui \
        -lopencv_videoio

# Boost Configuration
BOOST_PATH = /usr/local
INCLUDEPATH += $$BOOST_PATH/include
LIBS += -L$$BOOST_PATH/lib \
        -L/usr/local/lib \
        -L/usr/lib/x86_64-linux-gnu \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_videoio \
        -lboost_system \
        -lboost_filesystem \
        -lrt  # shared memory library

SOURCES += \
    shared_memory_viewer.cpp

HEADERS += 