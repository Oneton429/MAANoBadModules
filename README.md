# MAANoBadModules

一个外部注入补丁，用于屏蔽 [MAA](https://github.com/MaaAssistantArknights/MaaAssistantArknights) 的“以下注入到MAA的DLL可能会导致MAA闪退或界面渲染异常”弹窗。

通过 Hook `kernel32!GetModuleHandleW`，让 MAA 内部的 `BadModules.GetBadInjectedModules()` 对白名单内的 DLL 始终得到"未加载"的结果，从而跳过弹窗。**不修改 MAA 源码或可执行文件。**

## 免责声明
> [!WARNING]
> 如果觉得手动点关闭很烦且**确定之前的注入不会带来什么问题**的话，可以使用本项目，但**治标不治本**。
> <img width="790" height="87" alt="image" src="https://github.com/user-attachments/assets/1828c3d9-b35f-415b-8d83-689285f9bdd8" />
>
> **请在明确知晓风险的前提下使用本项目！**

## 文件说明

| 文件 | 说明 |
|---|---|
| `MAALauncher.exe` | 启动器，替代 `MAA.exe` 使用，自动注入 |
| `MAANoBadModules.dll` | Hook DLL，静态链接 MinHook，无额外依赖 |
| `MAANoBadModules.txt` | 白名单配置文件，用户手动编辑 |

## 使用方法

1. 从 [Releases](https://github.com/Oneton429/MAANoBadModules/releases) 下载最新版本。
2. 将 `MAALauncher.exe`、`MAANoBadModules.dll`、`MAANoBadModules.txt` 三个文件放到 MAA 的安装目录（与 `MAA.exe` 同级）。
3. 编辑 `MAANoBadModules.txt`，将需要屏蔽的 DLL 名称写入（每行一个）。
4. 以后**双击 `MAALauncher.exe`** 代替原来的 `MAA.exe` 启动 MAA。

## 配置文件格式

```
# 以 '#' 开头的行是注释，空行会被忽略，大小写不敏感

NahimicOSD.dll
AudioDevProps2.dll
```

- 文件为空或不存在时，白名单为空，MAA 的原始检测行为**不受任何影响**。
- MAA 默认检测的四个 DLL 名称：`NahimicOSD.dll`、`AudioDevProps2.dll`、`GTII-OSD64.dll`、`GTIII-OSD64.dll`。

## 工作原理

```
MAALauncher.exe
  │  以 CREATE_SUSPENDED 启动 MAA.exe
  │  注入 MAANoBadModules.dll（hook 在任何托管代码前就位）
  └─ ResumeThread → MAA 正常运行
        │
        └─ GetModuleHandleW("NahimicOSD.dll") 等调用
             │
             └─ Hook 函数：在白名单内 → return NULL
                           不在白名单 → 透传原始函数
```

## 构建

依赖：CMake 3.20+、MSVC（Visual Studio 2019+）、x64 目标。

由于本项目使用 Git Submodule 管理 MinHook 依赖，在克隆代码时需要加上 `--recurse-submodules` 参数：

```powershell
git clone --recurse-submodules https://github.com/Oneton429/MAANoBadModules.git
cd MAANoBadModules
```

如果克隆时忘记加参数，可以随后执行：

```powershell
git submodule update --init
```

然后使用 CMake 构建：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

输出文件在 `build\Release\`，构建后会自动将默认配置文件 `MAANoBadModules.txt` 复制到该目录下。

## 依赖

- [MinHook v1.3.4](https://github.com/TsudaKageyu/minhook)

## 许可证

MIT，详见 [LICENSE](LICENSE)。
