#pragma once

#include "AVLog.h"
#include "AVTool.h"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>


///前向声明
struct AVFrame;
enum AVCodecID;

/// ==================== 错误处理 ====================

/**
 * @brief 打印FFmpeg错误信息
 * @param err FFmpeg错误码（负数）
 */
inline void PrintErr(int err)
{
    char buf[1024] = { 0 };
    av_strerror(err, buf, sizeof(buf) - 1);
    std::cerr << buf << std::endl;
}

/**
 * @brief 错误检查宏，如果错误则打印并退出
 */
#define CHECK_ERR(err) \
    if (err < 0)       \
    {                  \
        PrintErr(err); \
        getchar();     \
        return -1;     \
    }

/**
 * @brief 错误检查宏（用于函数内部，抛出异常）
 */
#define THROW_ERR(err)                            \
    if (err < 0)                                  \
    {                                             \
        PrintErr(err);                            \
        throw std::runtime_error("FFmpeg error"); \
    }

//////////////////////////////////////////////////////////////////


// ==================== 通用工厂宏定义 ====================

namespace fs = std::filesystem;

/// 声明共享指针创建工厂
#define DECLARE_CREATE(ClassName)           \
public:                                     \
    using Ptr = std::shared_ptr<ClassName>; \
    static auto create() -> ClassName::Ptr;

/// 声明独占指针创建工厂
#define DECLARE_CREATE_UN(ClassName)        \
public:                                     \
    using Ptr = std::unique_ptr<ClassName>; \
    static auto create() -> ClassName::Ptr;

/// 声明共享指针默认创建工厂（支持可变参数）
#define DECLARE_CREATE_DEFAULT(ClassName)   \
public:                                     \
    using Ptr = std::shared_ptr<ClassName>; \
    static auto create() -> ClassName::Ptr; \
    template <typename... Args>             \
    static auto create(Args &&...args) -> ClassName::Ptr;

/// 声明独占指针默认创建工厂（支持可变参数）
#define DECLARE_CREATE_UN_DEFAULT(ClassName) \
public:                                      \
    using Ptr = std::unique_ptr<ClassName>;  \
    static auto create() -> ClassName::Ptr;  \
    template <typename... Args>              \
    static auto create(Args &&...args) -> ClassName::Ptr;

/// 声明克隆接口（用于多态对象）
#define DECLARE_CLONEABLE_UN(ClassName) \
public:                                 \
    virtual auto clone() const -> std::unique_ptr<ClassName> = 0;

#define DECLARE_CLONE(ClassName) \
public:                          \
    auto clone() const -> ClassName::Ptr;

/// 声明工厂接口（抽象工厂模式）
#define DECLARE_FACTORY(InterfaceName)                       \
public:                                                      \
    using FactoryPtr       = std::shared_ptr<InterfaceName>; \
    using FactoryUniquePtr = std::unique_ptr<InterfaceName>; \
    static auto create() -> FactoryPtr;                      \
    static auto createUnique() -> FactoryUniquePtr;

#define DECLARE_CREATE_CLONE(ClassName) \
    DECLARE_CREATE(ClassName)           \
    DECLARE_CLONE(ClassName)

#define DECLARE_CREATE_CLONE_UN(ClassName) \
    DECLARE_CREATE_UN(ClassName)           \
    DECLARE_CLONE(ClassName)

// ==================== 实现宏定义 ====================

/// 实现共享指针创建工厂
#define IMPLEMENT_CREATE(ClassName)            \
    auto ClassName::create() -> ClassName::Ptr \
    {                                          \
        return std::make_shared<ClassName>();  \
    }

/// 实现独占指针创建工厂
#define IMPLEMENT_CREATE_UN(ClassName)         \
    auto ClassName::create() -> ClassName::Ptr \
    {                                          \
        return std::make_unique<ClassName>();  \
    }

/// 实现共享指针默认创建工厂（支持可变参数）
#define IMPLEMENT_CREATE_DEFAULT(ClassName)                              \
    inline auto ClassName::create() -> ClassName::Ptr                    \
    {                                                                    \
        return std::make_shared<ClassName>();                            \
    }                                                                    \
    template <typename... Args>                                          \
    inline auto ClassName::create(Args &&...args) -> ClassName::Ptr      \
    {                                                                    \
        return std::make_shared<ClassName>(std::forward<Args>(args)...); \
    }

/// 实现独占指针默认创建工厂（支持可变参数）
#define IMPLEMENT_CREATE_UN_DEFAULT(ClassName)                           \
    inline auto ClassName::create() -> ClassName::Ptr                    \
    {                                                                    \
        return std::make_unique<ClassName>();                            \
    }                                                                    \
    template <typename... Args>                                          \
    inline auto ClassName::create(Args &&...args) -> ClassName::Ptr      \
    {                                                                    \
        return std::make_unique<ClassName>(std::forward<Args>(args)...); \
    }

/// 实现克隆接口
#define IMPLEMENT_CLONE(ClassName)                  \
    auto ClassName::clone() const -> ClassName::Ptr \
    {                                               \
        return std::make_shared<ClassName>(*this);  \
    }

#define IMPLEMENT_CLONE_UN(ClassName)               \
    auto ClassName::clone() const -> ClassName::Ptr \
    {                                               \
        return std::make_unique<ClassName>(*this);  \
    }

/// 实现模板克隆（用于派生类）
#define IMPLEMENT_CLONE_TEMPLATE(ClassName, BaseClass)          \
    auto ClassName::clone() const -> std::unique_ptr<BaseClass> \
    {                                                           \
        return std::make_unique<ClassName>(*this);              \
    }

#define IMPLEMENT_CREATE_CLONE(ClassName) \
    IMPLEMENT_CREATE(ClassName)           \
    IMPLEMENT_CLONE(ClassName)

#define IMPLEMENT_CREATE_CLONE_UN(ClassName) \
    IMPLEMENT_CREATE_UN(ClassName)           \
    IMPLEMENT_CLONE_UN(ClassName)

// ==================== 高级工厂宏定义 ====================

/// 声明带别名的工厂（支持多种指针类型）
#define DECLARE_FACTORY_WITH_ALIASES(ClassName)            \
public:                                                    \
    using SharedPtr = std::shared_ptr<ClassName>;          \
    using UniquePtr = std::unique_ptr<ClassName>;          \
    using WeakPtr   = std::weak_ptr<ClassName>;            \
    static auto createShared() -> SharedPtr;               \
    static auto createUnique() -> UniquePtr;               \
    template <typename... Args>                            \
    static auto createShared(Args &&...args) -> SharedPtr; \
    template <typename... Args>                            \
    static auto createUnique(Args &&...args) -> UniquePtr;

/// 实现带别名的工厂
#define IMPLEMENT_FACTORY_WITH_ALIASES(ClassName)                        \
    auto ClassName::createShared() -> ClassName::SharedPtr               \
    {                                                                    \
        return std::make_shared<ClassName>();                            \
    }                                                                    \
    auto ClassName::createUnique() -> ClassName::UniquePtr               \
    {                                                                    \
        return std::make_unique<ClassName>();                            \
    }                                                                    \
    template <typename... Args>                                          \
    auto ClassName::createShared(Args &&...args) -> ClassName::SharedPtr \
    {                                                                    \
        return std::make_shared<ClassName>(std::forward<Args>(args)...); \
    }                                                                    \
    template <typename... Args>                                          \
    auto ClassName::createUnique(Args &&...args) -> ClassName::UniquePtr \
    {                                                                    \
        return std::make_unique<ClassName>(std::forward<Args>(args)...); \
    }

// ==================== 单例模式宏定义 ====================

/// 声明线程安全的单例模式（Meyer's Singleton）
#define DECLARE_SINGLETON(ClassName)                  \
public:                                               \
    static auto instance() -> ClassName &;            \
    ClassName(const ClassName &)            = delete; \
    ClassName &operator=(const ClassName &) = delete; \
    ClassName(ClassName &&)                 = delete; \
    ClassName &operator=(ClassName &&)      = delete; \
                                                      \
private:                                              \
    ClassName();                                      \
    ~ClassName();

/// 实现线程安全的单例模式
#define IMPLEMENT_SINGLETON(ClassName)        \
    auto ClassName::instance() -> ClassName & \
    {                                         \
        static ClassName instance;            \
        return instance;                      \
    }

/// 声明单例工厂（返回共享指针）
#define DECLARE_SINGLETON_FACTORY(ClassName)          \
public:                                               \
    using Ptr = std::shared_ptr<ClassName>;           \
    static auto instance() -> Ptr;                    \
    ClassName(const ClassName &)            = delete; \
    ClassName &operator=(const ClassName &) = delete; \
    ClassName(ClassName &&)                 = delete; \
    ClassName &operator=(ClassName &&)      = delete; \
                                                      \
private:                                              \
    ClassName();                                      \
    ~ClassName();                                     \
    friend struct SingletonCreator;

/// 实现单例工厂
#define IMPLEMENT_SINGLETON_FACTORY(ClassName)                \
    auto ClassName::instance() -> ClassName::Ptr              \
    {                                                         \
        static auto instance = std::make_shared<ClassName>(); \
        return instance;                                      \
    }

// ==================== 使用示例 ====================

/*
示例1：基本使用

class MyClass {
    DECLARE_CREATE_UN(MyClass)
private:
    MyClass() = default;
};

IMPLEMENT_CREATE_UN(MyClass)

// 使用：
auto obj = MyClass::create();

示例2：带参数的使用

class Config {
    DECLARE_CREATE_UN_DEFAULT(Config)
public:
    Config(const std::string& path, int timeout);
private:
    Config() = delete;
};

IMPLEMENT_CREATE_UN_DEFAULT(Config)

// 使用：
auto config = Config::create("config.json", 30);

示例3：克隆模式

class Shape {
    DECLARE_CLONEABLE(Shape)
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
};

class Circle : public Shape {
public:
    DECLARE_CLONEABLE(Circle)
    Circle(double radius) : radius_(radius) {}
    double area() const override { return 3.14 * radius_ * radius_; }
    auto clone() const -> std::unique_ptr<Circle> override {
        return std::make_unique<Circle>(*this);
    }
private:
    double radius_;
};

// 使用多态克隆：
std::unique_ptr<Shape> shape = std::make_unique<Circle>(5.0);
auto cloned = shape->clone();

示例4：完整工厂

class Connection {
    DECLARE_FACTORY_WITH_ALIASES(Connection)
public:
    Connection(const std::string& url, int port);
private:
    Connection() = delete;
};

IMPLEMENT_FACTORY_WITH_ALIASES(Connection)

// 使用：
auto conn1 = Connection::createShared("localhost", 8080);
auto conn2 = Connection::createUnique("example.com", 443);

示例5：单例模式

class Logger {
    DECLARE_SINGLETON(Logger)
public:
    void log(const std::string& message);
};

IMPLEMENT_SINGLETON(Logger)

// 使用：
Logger::instance().log("Hello, world!");
*/
