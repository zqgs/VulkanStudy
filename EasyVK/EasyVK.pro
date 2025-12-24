QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        EasyVulkan.cpp \
        VKBase+.cpp \
        VKBase.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


macx {
    # 定义平台后缀
    PLATFORM = macOS
    # macOS Vulkan SDK
    VULKAN_SDK_PATH = /Users/zengqingguo/VulkanSDK/1.4.321.0/macOS

    INCLUDEPATH += $$VULKAN_SDK_PATH/include
    LIBS += -L$$VULKAN_SDK_PATH/lib -lMoltenVK -lglfw3 -lvulkan

    LIBS += -framework Cocoa \
            -framework IOKit \
            -framework CoreVideo \
            -framework CoreFoundation \
            -framework CoreGraphics \
            -framework Metal \
            -framework QuartzCore \
            -framework OpenGL

    QMAKE_APPLE_DEVICE_ARCHS = x86_64
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 12.0

    HEADERS += VulkanSurface.h
    SOURCES += VulkanSurface.mm
}

win32 {
    # 定义平台后缀
    PLATFORM = Win64
    # Windows Vulkan SDK
    VULKAN_SDK_PATH = E:/VulkanSDK/1.3.290.0

    INCLUDEPATH += $$VULKAN_SDK_PATH/Include
    LIBS += -L$$VULKAN_SDK_PATH/Lib -lvulkan-1 -lglfw3

    # Windows-specific libraries (for example)
    LIBS += -lgdi32 -luser32 -lkernel32 -lShell32
}

unix:!macx {
     # 定义平台后缀
    PLATFORM = Linux
}
# 构建类型后缀
CONFIG(release, debug|release) {
    BUILDTYPE = release
} else {
    BUILDTYPE = debug
}

BASE_DIR = $$PWD/build/$${PLATFORM}/$${BUILDTYPE}

DLLDESTDIR = $$BASE_DIR
DESTDIR    = $$BASE_DIR
OBJECTS_DIR = $$BASE_DIR/objectdir
MOC_DIR     = $$BASE_DIR/mocdir


HEADERS += \
    EasyVKStart.h \
    EasyVulkan.h \
    GlfwGeneral.h \
    VKBase+.h \
    VKBase.h \
    VKFormat.h

DISTFILES += \
    shader/FirstTriangle.frag.shader \
    shader/FirstTriangle.vert.shader \
    shader/PushConstant.vert.shader \
    shader/UniformAndShaderStorage.vert.shader \
    shader/UniformBuffer.vert.shader \
    shader/VertexBuffer.frag.shader \
    shader/VertexBuffer.vert.shader
