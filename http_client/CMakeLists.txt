# ============================================================================
# http_client 模块的 CMakeLists.txt
# ============================================================================
#
# 【这个文件是干什么的】
#   顶层 CMakeLists.txt 负责"组织项目"，本文件负责"定义这一个模块的 target"。
#   target = 一个构建产物（这里是可执行文件 http_client.exe）。
#
#   两个文件的分工：
#     顶层 (../CMakeLists.txt)   project() + add_subdirectory("http_client")
#     本文件                      add_executable + 各种 target_xxx
#
#   ★ 规则：4.9 节讲的"三件事"（源文件、自己的头文件、第三方库），
#     永远写在 add_executable 所在的那个文件里 —— 也就是本文件。
#     顶层那份只负责"把子目录拉进来"。
#
# 【现代 CMake 的核心形状】
#   定义一个 target，然后给这个 target 一项一项地加需求。
#   所有命令都长这样：target_xxx(<哪个target> <作用域> <内容>)
# ============================================================================


# ---------------------------------------------------------------------------
# 1. 定义可执行文件 target，并列出它的源文件
# ---------------------------------------------------------------------------
# 命令：add_executable(<target 名字> <源文件...>)
#
#   参数含义：
#     第 1 个参数 http_client —— target 的名字，同时也是产物名。
#        在 Windows 上生成 http_client.exe。
#        ★ 这个名字全项目唯一，后面每条 target_xxx 命令都要用它。
#     后面的参数 —— 要编译进这个 target 的源文件，
#        路径**相对于本文件所在目录**（也就是 http_client/ 这一层）。
#
#   为什么 source/http_client.cpp 要带 source/ 前缀：
#     该文件实际在 http_client/source/ 下，不写前缀就找不到，
#     配置时会报 "Cannot find source file: http_client.cpp"。
#     （VS Code 模板生成的就是错的，少了这个前缀。）
#
#   为什么头文件（.h）不用列进来：
#     .h 不是编译单元。编译器只编译 .cpp，头文件是靠 #include "贴进来"的。
#     能不能找到头文件，由下面第 2 步的 target_include_directories 决定。

add_executable (http_client
	"main.cpp"
	"source/http_client.cpp"
)


# ---------------------------------------------------------------------------
# 2. 头文件搜索路径：告诉编译器"自己的头文件放在哪个目录"
# ---------------------------------------------------------------------------
# 命令：target_include_directories(<target> <作用域关键字> <目录...>)
#
#   为什么需要它：
#     main.cpp 里写的是 #include "http_client.h"，而该文件在 include/ 下。
#     不加这条，编译器只会在源文件所在目录等默认位置找，会报
#     "无法打开包括文件: http_client.h"。
#
#   参数 ${CMAKE_CURRENT_SOURCE_DIR}/include 的含义：
#     CMAKE_CURRENT_SOURCE_DIR = "当前这个 CMakeLists.txt 所在的目录"。
#     拼出来就是本模块的 include/ 目录。
#
#   ★ 给的是"目录"，不是"文件"：
#       ✅ ${CMAKE_CURRENT_SOURCE_DIR}/include
#       ❌ ${CMAKE_CURRENT_SOURCE_DIR}/include/http_client.h
#     include 搜索路径是一份"文件夹清单"，编译器会去这些文件夹里找
#     #include 提到的那个文件名。给成文件本身，它就会去找
#     "include/http_client.h/http_client.h" 这种不存在的路径。
#     （VS Code 模板生成的也正是这个错。）
#
#   为什么用 ${CMAKE_CURRENT_SOURCE_DIR} 而不写死绝对路径：
#     写死 D:/BaiduSyncdisk/... 的话，项目一挪位置、一换电脑就废。
#
#   PRIVATE 是什么（必填，三选一，漏写会直接报
#   "target_include_directories called with invalid arguments"）：
#     PRIVATE   只有我自己编译时用，不传给使用我的 target
#     PUBLIC    我自己用，用我的人也必须跟着用
#     INTERFACE 我不用，但用我的人必须用
#   判定口诀："用我的人需不需要知道这件事？"
#   本项目是可执行文件，是依赖图的终点，没有使用者 → PRIVATE。

target_include_directories(
	http_client
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
)


# ---------------------------------------------------------------------------
# 3. 第三方库（vcpkg 装的）
# ---------------------------------------------------------------------------
# ★ 用任何库，永远都是这两步，一句不能少：
#
#     ① find_package(库名 CONFIG REQUIRED)          —— 找到库在哪
#     ② target_link_libraries(我的target PRIVATE 库名::target名)
#                                                    —— 把它接到自己身上
#
#   为什么必须写两句？它们管的事不同：
#     find_package     只是"**知道**这个库在哪了"，还没用到自己身上。
#     target_link_libraries 才把库的头文件路径、它依赖的其它库，
#                      **全部接到自己的编译和链接命令里**。
#
#   为什么不用手写 include 路径？
#     因为库也是 target。httplib::httplib 这个 target 自己"记得"：
#       我的头文件在 <vcpkg>\installed\x64-windows\include
#       我依赖 zlib、brotli、ws2_32、crypt32 ...
#     链接之后 CMake 会把这些信息自动传播过来。
#     想验证：type out\build\x64-debug\build.ninja | findstr INCLUDES
#
#   target 名从哪来？
#     装库时 vcpkg 会打印出来，照抄即可：
#       cpp-httplib provides CMake targets:
#           find_package(httplib CONFIG REQUIRED)
#           target_link_libraries(main PRIVATE httplib::httplib)
#
#   带 :: 的名字（httplib::httplib）是约定：表示"这一定是个 target"，
#   写错会立刻报错，而不会悄悄去磁盘上找个同名文件。

# --- 库 1：cpp-httplib，用来发 HTTP 请求 ---

#	CONFIG找库的方式，读库自己带的 <包名>Config.cmake 文件，里面明确写了自己是啥
find_package(httplib CONFIG REQUIRED)
target_link_libraries(http_client PRIVATE httplib::httplib)

# --- 库 2：nlohmann_json，用来解析 API 返回的 JSON ---
# 解析 API 返回的 JSON 用。加库永远是这两句，和 httplib 完全一样的套路。
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(http_client PRIVATE nlohmann_json::nlohmann_json)


# ---------------------------------------------------------------------------
# 4. C++ 标准
# ---------------------------------------------------------------------------
# set_property(TARGET ... PROPERTY CXX_STANDARD 20) 是"给这个 target 设属性"，
# 等价于声明"C++ 标准用 20"。只影响本 target，不污染别的模块。
#
# 外面那层 if (CMAKE_VERSION VERSION_GREATER 3.12) 是"版本保护"：
#   CXX_STANDARD 属性从 CMake 3.1 就有，这个判断偏保守，删掉也不会出错。
#
# ★ 更现代的等价写法是（推荐以后用这个）：
#       target_compile_features(http_client PRIVATE cxx_std_20)
#   好处：CMake 会自动翻译成正确的编译器参数（MSVC 是 /std:c++20，
#   GCC 是 -std=c++20），而且将来把模块改成库时，把 PRIVATE 换成 PUBLIC
#   就能让使用者自动继承这个标准要求。

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET http_client PROPERTY CXX_STANDARD 20)
endif()


# ---------------------------------------------------------------------------
# 5. 编译选项
# ---------------------------------------------------------------------------
# 命令：target_compile_options(<target> <作用域> <选项...>)
#
#   参数 /utf-8 的含义（MSVC 的开关）：
#     告诉编译器"源文件是 UTF-8"，并且运行期字符集也用 UTF-8。
#     本项目源码里有中文（注释和 std::cout 的字符串），不加它会出：
#       warning C4819: 该文件包含不能在当前代码页(936)中表示的字符
#       error C2001: 字符串字面量中的换行符   ← 明明代码是合法的一行
#
#   ★ 注意：/utf-8 是 MSVC 专用，GCC/Clang 不认。
#     本项目只用 MSVC，所以直接写没问题。
#     想跨平台要用生成表达式 $<...>：
#       target_compile_options(http_client PRIVATE
#           $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive->
#           $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)
#
#   另外提醒：控制台显示中文还需要程序里调一次
#     SetConsoleOutputCP(CP_UTF8);
#   否则中文在 GBK(936) 控制台上会显示成乱码。/utf-8 只管编译期。

# TODO: 如有需要，请添加测试并安装目标。
target_compile_options(http_client PRIVATE /utf-8)


# ---------------------------------------------------------------------------
# 6. 以后要加东西时怎么改（速查）
# ---------------------------------------------------------------------------
#   加源文件        →  在 add_executable 的参数里加一行
#   加自己的头文件目录 →  在 target_include_directories 里加一个目录
#   加第三方库      →  find_package + target_link_libraries 各一行
#   加编译选项/宏   →  target_compile_options / target_compile_definitions
#
#   改完不用手动 configure，直接：
#       cmake --build out\build\x64-debug
#   （CMake 会自己发现本文件变了并先重新配置）
