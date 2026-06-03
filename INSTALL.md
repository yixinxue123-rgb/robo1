# Armor Detector C++ 环境配置指南

## 1. 安装必备工具

### CMake (必需)
- **下载**: https://cmake.org/download/
- **Windows**: 下载 `.msi` 安装包并运行
- 安装时选择 **"Add CMake to the system PATH"**

### C++ 编译器 (必需，选择其一)

#### 选项 A: MSVC (Visual Studio)
- 安装 **Visual Studio 2022** 或更新版本
- 在安装时选择 **"Desktop development with C++"** 组件
- 包含 MSVC 编译器和 Windows SDK

#### 选项 B: MinGW-w64 (GCC)
- **下载**: https://github.com/msys2/msys2-installer/releases
- 或使用 **MSYS2**:
  ```
  pacman -S mingw-w64-x86_64-gcc
  ```
- 确保 `g++.exe` 在 PATH 中

### OpenCV (必需)

#### 选项 A: 预编译版本 (推荐)
1. 下载 OpenCV for Windows: https://opencv.org/releases/
2. 选择 **Windows** 版本下载
3. 解压到某个目录 (如 `C:\opencv`)
4. 设置环境变量 `OpenCV_DIR` = `C:\opencv\build`
5. 将 `C:\opencv\build\x64\vc15\bin` 添加到 `PATH`

#### 选项 B: 使用 vcpkg (自动安装)
```cmd
# 安装 vcpkg (如果未安装)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# 安装 OpenCV
.\vcpkg install opencv
```

#### 选项 C: 使用 Chocolatey (包管理器)
```cmd
choco install opencv
```

## 2. 配置项目

### 方法 A: 使用批处理脚本 (推荐)
```cmd
cd armor_detector
build.bat
```

### 方法 B: 手动构建
```cmd
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 方法 C: 使用 Visual Studio
1. 打开 CMakeLists.txt 文件
2. Visual Studio 会自动检测 CMake 项目
3. 选择构建配置 (Release/Debug)
4. 点击构建

## 3. 运行程序

```cmd
# 如果使用默认图片名
armor_detector.exe

# 或指定图片文件
armor_detector.exe armor.jpg
armor_detector.exe 你的图片.jpg
```

## 4. 项目结构

```
armor_detector/
├── CMakeLists.txt      # CMake 配置文件
├── main.cpp            # 主程序源码
├── build.bat           # 构建脚本 (Windows)
└── INSTALL.md          # 本安装指南
```

## 5. 测试安装

### 测试 CMake 安装
```cmd
cmake --version
```

### 测试编译器
```cmd
# 测试 GCC
g++ --version

# 测试 MSVC
cl
```

### 测试 OpenCV 安装
```cmd
# 如果使用 vcpkg
echo %VCPKG_ROOT%

# 或检查 OpenCV 目录
dir C:\opencv
```

## 6. 故障排除

### CMake 找不到 OpenCV
1. 设置环境变量 `OpenCV_DIR`
2. 或修改 CMakeLists.txt:
   ```cmake
   set(OpenCV_DIR "C:/opencv/build")
   find_package(OpenCV REQUIRED)
   ```

### 编译错误
- 确保安装了正确的 C++ 标准库
- 检查 OpenCV 版本兼容性
- 确保所有头文件路径正确

### 链接错误
- 确保 OpenCV 库文件路径在链接器搜索路径中
- 检查库文件名称和版本

## 7. 快速安装脚本 (可选)

创建 `install_deps.bat`:
```batch
@echo off
echo 安装 Armor Detector 依赖...

echo 1. 安装 Chocolatey (如果未安装)
powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))"

echo 2. 安装 CMake
choco install cmake -y

echo 3. 安装 OpenCV
choco install opencv -y

echo 4. 安装 MinGW (如果需要)
choco install mingw -y

echo 安装完成！
pause
```

## 8. 使用 Docker (替代方案)

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libopencv-dev \
    git

WORKDIR /app
COPY . .
RUN mkdir build && cd build && \
    cmake .. && make

CMD ["./build/armor_detector"]
```

## 9. 支持的系统
- Windows 10/11 (推荐)
- Linux (需要适配 CMake)
- macOS (需要 Homebrew 安装 OpenCV)

## 联系方式
如有问题，请检查:
1. 所有工具是否已正确安装
2. 环境变量是否设置正确
3. OpenCV 版本是否兼容