#ifndef EASYVKSTART_H
#define EASYVKSTART_H

//可能会用上的C++标准库
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <numeric>
#include <cassert>
#include <mutex>
#include <thread>

//qt
#include <QDebug>
#include <QString>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QCoreApplication>

//GLM
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
//如果你惯用左手坐标系，在此定义GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//stb_image.h
#include <stb_image/stb_image.h>


#define APP_PATH (qApp->applicationDirPath())
#ifdef __APPLE__ //MACOS
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_macos.h>
    #include "VulkanSurface.h"

    #define VK_GLSLC "/Users/zengqingguo/VulkanSDK/1.4.321.0/macOS/bin/glslc"
    #define CODE_DIR "/Users/zengqingguo/Desktop/gitHub/VulkanStudy/EasyVK"
#elif _WIN32  //Windows
    #define VK_USE_PLATFORM_WIN32_KHR    //在包含vulkan.h前定义该宏，会一并包含vulkan_win32.h和windows.h
    #define NOMINMAX                     //定义该宏可避免windows.h中的min和max两个宏与标准库中的函数名冲突
    #pragma comment(lib, "vulkan-1.lib") //链接编译所需的静态存根库
    #include <vulkan/vulkan.h>

    #define VK_GLSLC "E:/VulkanSDK/1.3.290.0/Bin/glslc.exe"
    #define CODE_DIR "D:/Works/Plan/VulkanLearn/VulkanStudy/EasyVK"
#endif


template<typename T>
class arrayRef {
    T* const pArray = nullptr;
    size_t count = 0;
public:
    //从空参数构造，count为0
    arrayRef() = default;
    //从单个对象构造，count为1
    arrayRef(T& data) :pArray(&data), count(1) {}
    //从顶级数组构造
    template<size_t ElementCount>
    arrayRef(T(&data)[ElementCount]) : pArray(data), count(ElementCount) {}
    //从指针和元素个数构造
    arrayRef(T* pData, size_t elementCount) :pArray(pData), count(elementCount) {}
    //若T带const修饰，兼容从对应的无const修饰版本的arrayRef构造
    arrayRef(const arrayRef<std::remove_const<T>>& other) :pArray(other.Pointer()), count(other.Count()) {}
    //Getter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }
    //Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }
    //Non-const Function
    //禁止复制/移动赋值（arrayRef旨在模拟“对数组的引用”，用处归根结底只是传参，故使其同C++引用的底层地址一样，防止初始化后被修改）
    arrayRef& operator=(const arrayRef&) = delete;
};

//这个宏用来把函数分割成能被多次执行，以及只执行一次的两个部分
#define ExecuteOnce(...) { static bool executed = false; if (executed) return __VA_ARGS__; executed = true; }

//----------Math Related-------------------------------------------------------
template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        std::is_signed<T>::value,
        int
    >::type = 0
>
static int GetSign(T num) {
    return (num > 0) - (num < 0);
}

template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        std::is_signed<T>::value,
        int
    >::type = 0
>
static bool SameSign(T num0, T num1) {
    return (num0 == num1 || !((num0 >= 0 && num1 <= 0) || (num0 <= 0 && num1 >= 0)));
}

template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        std::is_signed<T>::value,
        int
    >::type = 0
>
static bool SameSign_Weak(T num0, T num1) {
    return (num0 ^ num1) >= 0;
}

template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        std::is_signed<T>::value,
        int
    >::type = 0
>
static bool Between_Open(T min, T num, T max) {
    return ((min - num) & (num - max)) < 0;
}

template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value &&
        std::is_signed<T>::value,
        int
    >::type = 0
>
static bool Between_Closed(T min, T num, T max) {
    return ((num - min) | (max - num)) >= 0;
}

static bool compileShader(const QString& glslcPath,
                   const QString& shaderPath,
                   const QString& spvPath)
{
    QProcess process;

    // 构造参数：glslc -o out.spv shader.glsl
    QStringList args;
    args << "-o" << spvPath << shaderPath;

    process.setProgram(glslcPath);
    process.setArguments(args);

    // Qt 跨平台处理 PATH, working directory 不需要设置
    process.start();
    if (!process.waitForStarted()) {
        qDebug() << "Failed to start glslc:" << process.errorString();
        return false;
    }

    if (!process.waitForFinished()) {
        qDebug() << "glslc timeout:" << process.errorString();
        return false;
    }

    // 输出编译器 log（警告/错误）
    QString stdOut = process.readAllStandardOutput();
    QString stdErr = process.readAllStandardError();

    if (!stdOut.isEmpty())
        qDebug() << "[glslc output]:" << stdOut.trimmed();

    if (!stdErr.isEmpty())
        qDebug() << "[glslc error ]:" << stdErr.trimmed();

    // glslc 返回 0 表示成功
    if (process.exitCode() != 0) {
        qDebug() << "Shader compilation failed. ExitCode =" << process.exitCode();
        return false;
    }

    qDebug() << "Compiled successfully:" << shaderPath << "->" << spvPath;
    return true;
}


#endif // EASYVKSTART_H

