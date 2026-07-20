# 项目介绍

## 项目配置
    c++20

## 如何 clone 项目
    git clone --recursive [InventEngine2](https://github.com/chichi-9527/InventEngine2.git)

### 若使用了 git clone
    1. 进入项目目录
    2. git submodule init
    3. git submodule update --init --recursive

## 构建项目
    1. 进入项目目录 ./
```bash
    mkdir build
    cd build
    cmake -G "Visual Studio 17 2022" -A x64 -T host=x64 ..
```
    5. 双击 build 文件夹中 InventEngine2.sln 文件,即可打开 Visual Studio 2022 中构建项目


## 项目结构
    待完善

## 如何添加新文件

### 在Engine目录下添加新文件
    1. 在Engine/include/Public(Private)目录下添加头文件或头文件夹
    2. 在Engine/src目录下添加源文件或源文件夹
    3. 修改CMakeLists.txt文件(可以只修改注释),在 Visual Studio 中生成 ZERO_CHECK 项目
    4. 即可在 Visual Studio 中看到新添加的文件
### 其他项目同理

## 生成游戏项目
    1. 进入项目目录 ./Templates
    2. 执行 GenerateProject.ps1 脚本 ：

```powershell
    powershell -ExecutionPolicy Bypass -File .\GenerateProject.ps1 -ProjectName "yourGameName" -TargetDir "yourGameDir"
```

    3. 进入 yourGameDir 目录,即可看到新生成的游戏项目目录
    4. 使用 cmake 配置项目：
```bash
    mkdir build
    cd build
    cmake -G "Visual Studio 17 2022" -A x64 -T host=x64 ..
```
    5. 双击 build 文件夹中 yourGameName.sln 文件,即可打开 Visual Studio 2022 中构建项目
