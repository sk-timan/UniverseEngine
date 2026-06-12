## 新增需求

### 需求:系统必须提供基于 libclang 的反射代码生成器
系统必须提供构建期工具 `ReflectionGenerator`，使用 libclang 解析 `compile_commands.json` 指定的编译参数，从头文件 AST 提取 `UCLASS`/`USTRUCT` 标记类的继承关系、字段、方法与内存偏移。

#### 场景:扫描标记类并生成文件
- **当** 头文件包含 `GENERATED_BODY()` 且类声明带 `UCLASS()` 或 `USTRUCT()`
- **那么** 生成器必须输出对应的 `ClassName.generated.h` 与 `ClassName.gen.cpp` 到指定输出目录

#### 场景:未标记类不生成
- **当** 头文件不含反射宏标记
- **那么** 生成器必须跳过该文件，禁止生成空注册代码

### 需求:生成器必须用独立 Parser 解析宏参数
系统必须使用宏参数 Lexer/Parser（方案 B）从源文件文本解析 `UPROPERTY`、`UFUNCTION`、`UCLASS` 的参数，提取 `EditAnywhere`、`SaveGame`、`Category`、`meta` 等标志；禁止要求开发者改用 `annotate` 或自定义 attribute 替代 UE 风格宏。

#### 场景:UPROPERTY 标志关联到字段
- **当** 某字段声明前一行（或向上跳过空行/注释后）存在 `UPROPERTY(EditAnywhere, Category="Transform")`
- **那么** 生成器必须将该标志集关联到该字段，并写入生成代码中的属性描述符

#### 场景:宏参数解析失败阻断构建
- **当** 宏参数存在无法解析的语法（如未闭合括号）
- **那么** 生成器必须以非零退出码失败，并报告文件名与行号

### 需求:生成代码必须注册完整元数据
生成的 `.gen.cpp` 必须为每个属性提供名称、类型、offset、标志位与 metadata；必须为每个 `UFUNCTION` 提供函数指针绑定；必须在静态初始化阶段完成 `UClass` 注册。

#### 场景:STRUCT_OFFSET 与 AST 一致
- **当** 生成器为字段生成 offset
- **那么** offset 必须与 libclang 在同一编译配置下计算的字段偏移一致

#### 场景:GENERATED_BODY 展开可用
- **当** 开发者 include `ClassName.generated.h`
- **那么** `GENERATED_BODY()` 必须正确展开为 `StaticClass()` 声明与注册友元，且项目可成功编译链接

### 需求:libclang 必须以 vendored 预编译包提供
系统必须在 `OpenSpecTest/thirdparty/libclang/` 提供 vendored libclang 头文件、import lib 与 `libclang.dll`；`ReflectionGenerator` 必须链接该 vendored 包；禁止要求开发者本机安装 LLVM 或使用 `find_package(Clang)`；`OpenSpecTest` 主程序禁止链接 libclang。

#### 场景:干净环境可构建生成器
- **当** 开发者克隆仓库且未安装系统 LLVM
- **那么** 构建必须能成功编译 `ReflectionGenerator`，前提是 `thirdparty/libclang/` 内容完整

#### 场景:生成器可加载 libclang.dll
- **当** CMake 运行 `ReflectionGenerator` 进行代码生成
- **那么** 系统必须确保 `libclang.dll` 对生成器进程可加载（同目录复制或等效机制），禁止因 DLL 缺失导致 AST 解析静默失败

### 需求:构建系统必须集成代码生成步骤
CMake 必须在编译 `OpenSpecTest` 目标前运行 `ReflectionGenerator`；必须导出或消费 `compile_commands.json`；必须将生成目录加入 include 路径并将 `.gen.cpp` 编入目标。

#### 场景:头文件变更触发重新生成
- **当** 被扫描的反射头文件内容发生变更并触发构建
- **那么** 构建系统必须重新运行生成器并更新生成物后再编译

#### 场景:生成器失败阻止编译
- **当** `ReflectionGenerator` 退出码非零
- **那么** 构建必须中止，禁止使用过期的生成代码继续编译

## 修改需求

无。

## 移除需求

无。
