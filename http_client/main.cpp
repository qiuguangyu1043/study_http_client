// ===========================================================================
// main.cpp —— 用 cpp-httplib 请求 Open-Meteo 天气 API（中国城市）
// ===========================================================================
//
// 【这个程序演示了 4 件事】
//   1. 怎么发一个带查询参数的 GET 请求（httplib::Params）
//   2. 怎么区分"网络层失败"和"HTTP 层失败"
//   3. 怎么解析返回的 JSON（nlohmann/json）
//   4. 怎么让控制台正确显示中文（SetConsoleOutputCP）
//
// 【依赖的库】（都在 CMakeLists.txt 里用 find_package + target_link_libraries 链接）
//   cpp-httplib    发 HTTP 请求
//   nlohmann-json  解析 JSON
//
// 【API 说明】
//   Open-Meteo 是免费天气 API，不需要 key。
//   文档: https://open-meteo.com/en/docs
// ===========================================================================

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// 简化写法。注意：只在 .cpp 里用，不要写在头文件里（原因见 4.10 节）
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// 要查询的城市：换城市只改这两个数字
// ---------------------------------------------------------------------------
struct City {
    const char* name;
    const char* latitude;
    const char* longitude;
};

// 常用中国城市坐标，想换城市改这里就行
constexpr City kCity{"北京", "39.9042", "116.4074"};


void self_print_weather(const City *citylist,size_t size, httplib::Client cli) {
	for (size_t i=0; i<size; ++i) {

    httplib::Params params = {
        {"latitude",  citylist[i].latitude},
        {"longitude", citylist[i].longitude},

        // current：只要"当前时刻"的值（每个变量一个数）
        {"current",   "temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"},

        // hourly：逐小时数组（今天 0 点到 23 点，共 24 个点）
        {"hourly",    "temperature_2m,relative_humidity_2m,wind_speed_10m,precipitation_probability"},

        // ★ 时区：不加的话返回的时间是 GMT，和北京时间差 8 小时
        {"timezone",  "Asia/Shanghai"},

        // 只要 1 天的逐小时数据（不写默认给 7 天 = 168 个点，太长）
        {"forecast_days", "1"},
    };

    // 公共 API 建议带一个 User-Agent，有些服务会拒绝没有 UA 的请求
    httplib::Headers headers = {
        {"User-Agent", "http_client-learning/1.0"},
    };

    std::cout << "正在请求 Open-Meteo ,city: " << citylist[i].name << "\n";
        auto res = cli.Get("/v1/forecast", params, headers);

        // ④ 两类失败必须分开判断（最重要的一步）
 // ---------------------------------------------------------------------
        if (!res) {
            // 网络层失败：连不上、超时、DNS 解析失败 —— 根本没拿到 HTTP 响应
            std::cerr << "网络错误: " << httplib::to_string(res.error()) << "\n";
            continue;
        }
        if (res->status != 200) {
            // HTTP 层失败：服务器收到了请求并回复了，但说"不行"
            std::cerr << "服务端返回 " << res->status << "\n" << res->body << "\n";
            continue;
        }

        // ---------------------------------------------------------------------
        // ⑤ 解析 JSON
        // ---------------------------------------------------------------------
        // json::parse 在格式非法时会抛异常，所以用 try 包住
        try {
            const json j = json::parse(res->body);

            // API 返回的是"最近网格点"的实际位置，不一定和请求的坐标完全相同
            std::cout << "\n请求坐标: " << kCity.latitude << ", " << kCity.longitude << "\n";
            std::cout << "实际坐标: " << j["latitude"] << ", " << j["longitude"]
                << "   时区: " << j["timezone"] << "\n";

            // ---- 当前天气 ----
            // current 是一个对象，每个变量直接给一个数值
            // & 表示引用——给已有对象起个"别名"，不复制,不占用内存
            const json& cur = j["current"];
            const json& cu = j["current_units"];

            std::cout << "\n【当前天气】" << cur["time"].get<std::string>() << "\n";
            std::cout << "  气温  : " << cur["temperature_2m"]
                << " " << cu["temperature_2m"].get<std::string>() << "\n";
            std::cout << "  相对湿度: " << cur["relative_humidity_2m"]
                << " " << cu["relative_humidity_2m"].get<std::string>() << "\n";
            std::cout << "  风速  : " << cur["wind_speed_10m"]
                << " " << cu["wind_speed_10m"].get<std::string>() << "\n";

            // ---- 逐小时预报 ----
            // hourly 里每个变量都是"数组"，用 .size() 拿长度，用 [i] 取第 i 个
            const json& h = j["hourly"];
            const json& hu = j["hourly_units"];

            const std::string t_unit = hu["temperature_2m"].get<std::string>();
            const std::string h_unit = hu["relative_humidity_2m"].get<std::string>();
            const std::string w_unit = hu["wind_speed_10m"].get<std::string>();

            std::cout << "\n【逐小时预报】共 " << h["time"].size() << " 个时间点\n";
            std::cout << "  时间              气温(" << t_unit << ")  湿度(" << h_unit
                << ")  风速(" << w_unit << ")  降水概率(%)\n";

            for (std::size_t i = 0; i < h["time"].size(); ++i) {
                // 时间形如 "2026-09-27T00:00"，取出 "00:00" 更好看
                std::string t = h["time"][i].get<std::string>();
                const auto pos = t.find('T');
                if (pos != std::string::npos) {
                    t = t.substr(pos + 1);
                }

                std::cout << "  " << t
                    << "     " << h["temperature_2m"][i]
                    << "     " << h["relative_humidity_2m"][i]
                    << "     " << h["wind_speed_10m"][i]
                    << "     " << h["precipitation_probability"][i]
                    << "\n";
            }
        }
        catch (const json::exception& e) {
            // JSON 解析失败：把原始响应打出来，方便排查
            std::cerr << "JSON 解析失败: " << e.what() << "\n";
            std::cerr << "原始响应:\n" << res->body << "\n";
            continue;
        }


		
	}

	std::cout << "\n=== 完成 ===\n";

}













constexpr City citys[] = {
	{"北京", "39.9042", "116.4074"},
	{"上海", "31.2304", "121.4737"},
	{"广州", "23.1291", "113.2644"},
	{"深圳", "22.5431", "114.0579"},
};



int main() {

#ifdef _WIN32
    // 关键：让本程序的控制台按 UTF-8 解释输出。
    // 不加这句，源码里的中文（UTF-8）在 GBK(936) 控制台上会显示成乱码。
    // 用这个 API 比手动 chcp 65001 更好：它跟着程序走，从 VS 里按 F5 运行也生效。
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "=== " << kCity.name << " 实时天气（Open-Meteo API）===\n";

    // ---------------------------------------------------------------------
    // ① 创建客户端
    // ---------------------------------------------------------------------
    // ★ 最重要的一点：Client 只接受"协议 + 主机名"，不能带路径和查询参数！
    //     对： "http://api.open-meteo.com"
    //     错： "http://api.open-meteo.com/v1/forecast?latitude=..."
    //   路径和参数要交给下面的 Get()。
    //
    // ★ 为什么是 http 而不是 https：
    //   当前 vcpkg 装的 httplib 只启用了 zlib，没启用 openssl，所以不支持 https。
    //   想用 https 需要：vcpkg install "cpp-httplib[openssl,zlib]:x64-windows"
    httplib::Client cli("http://api.open-meteo.com");

    // 超时设置：避免网络异常时程序卡死
    cli.set_connection_timeout(10);   // 建立连接最多等 10 秒
    cli.set_read_timeout(15);         // 读取响应最多等 15 秒

    // ---------------------------------------------------------------------
    // ② 组装查询参数
    // ---------------------------------------------------------------------
    // 用 httplib::Params 传参的好处：它会自动拼成 ?a=1&b=2，并自动做 URL 编码。
    // 所以这里写的是"原始值"，不需要自己写 %2F 之类。
    httplib::Params params = {
        {"latitude",  kCity.latitude},
        {"longitude", kCity.longitude},

        // current：只要"当前时刻"的值（每个变量一个数）
        {"current",   "temperature_2m,relative_humidity_2m,wind_speed_10m,weather_code"},

        // hourly：逐小时数组（今天 0 点到 23 点，共 24 个点）
        {"hourly",    "temperature_2m,relative_humidity_2m,wind_speed_10m,precipitation_probability"},

        // ★ 时区：不加的话返回的时间是 GMT，和北京时间差 8 小时
        {"timezone",  "Asia/Shanghai"},

        // 只要 1 天的逐小时数据（不写默认给 7 天 = 168 个点，太长）
        {"forecast_days", "1"},
    };

    // 公共 API 建议带一个 User-Agent，有些服务会拒绝没有 UA 的请求
    httplib::Headers headers = {
        {"User-Agent", "http_client-learning/1.0"},
    };

    // ---------------------------------------------------------------------
    // ③ 发请求
    // ---------------------------------------------------------------------
    // 注意这个重载的完整签名：Get(path, params, headers)
    //   - path    是 "/v1/forecast"（Client 里已经去掉了）
    //   - headers 是必填参数，不想传就写 {}
    //   写成 Get(path, params) 会报 C2665「没有重载函数可以转换所有参数类型」
    std::cout << "正在请求 Open-Meteo ...\n";
    auto res = cli.Get("/v1/forecast", params, headers);

    // ---------------------------------------------------------------------
    // ④ 两类失败必须分开判断（最重要的一步）
    // ---------------------------------------------------------------------
    if (!res) {
        // 网络层失败：连不上、超时、DNS 解析失败 —— 根本没拿到 HTTP 响应
        std::cerr << "网络错误: " << httplib::to_string(res.error()) << "\n";
        return 1;
    }
    if (res->status != 200) {
        // HTTP 层失败：服务器收到了请求并回复了，但说"不行"
        std::cerr << "服务端返回 " << res->status << "\n" << res->body << "\n";
        return 1;
    }

    // ---------------------------------------------------------------------
    // ⑤ 解析 JSON
    // ---------------------------------------------------------------------
    // json::parse 在格式非法时会抛异常，所以用 try 包住
    try {
        const json j = json::parse(res->body);

        // API 返回的是"最近网格点"的实际位置，不一定和请求的坐标完全相同
        std::cout << "\n请求坐标: " << kCity.latitude << ", " << kCity.longitude << "\n";
        std::cout << "实际坐标: " << j["latitude"] << ", " << j["longitude"]
                  << "   时区: " << j["timezone"] << "\n";

        // ---- 当前天气 ----
        // current 是一个对象，每个变量直接给一个数值
        // & 表示引用——给已有对象起个"别名"，不复制,不占用内存
        const json& cur = j["current"];
        const json& cu  = j["current_units"];

        std::cout << "\n【当前天气】" << cur["time"].get<std::string>() << "\n";
        std::cout << "  气温  : " << cur["temperature_2m"]
                  << " " << cu["temperature_2m"].get<std::string>() << "\n";
        std::cout << "  相对湿度: " << cur["relative_humidity_2m"]
                  << " " << cu["relative_humidity_2m"].get<std::string>() << "\n";
        std::cout << "  风速  : " << cur["wind_speed_10m"]
                  << " " << cu["wind_speed_10m"].get<std::string>() << "\n";

        // ---- 逐小时预报 ----
        // hourly 里每个变量都是"数组"，用 .size() 拿长度，用 [i] 取第 i 个
        const json& h  = j["hourly"];
        const json& hu = j["hourly_units"];

        const std::string t_unit  = hu["temperature_2m"].get<std::string>();
        const std::string h_unit  = hu["relative_humidity_2m"].get<std::string>();
        const std::string w_unit  = hu["wind_speed_10m"].get<std::string>();

        std::cout << "\n【逐小时预报】共 " << h["time"].size() << " 个时间点\n";
        std::cout << "  时间              气温(" << t_unit << ")  湿度(" << h_unit
                  << ")  风速(" << w_unit << ")  降水概率(%)\n";

        for (std::size_t i = 0; i < h["time"].size(); ++i) {
            // 时间形如 "2026-09-27T00:00"，取出 "00:00" 更好看
            std::string t = h["time"][i].get<std::string>();
            const auto pos = t.find('T');
            if (pos != std::string::npos) {
                t = t.substr(pos + 1);
            }

            std::cout << "  " << t
                      << "     " << h["temperature_2m"][i]
                      << "     " << h["relative_humidity_2m"][i]
                      << "     " << h["wind_speed_10m"][i]
                      << "     " << h["precipitation_probability"][i]
                      << "\n";
        }
    }
    catch (const json::exception& e) {
        // JSON 解析失败：把原始响应打出来，方便排查
        std::cerr << "JSON 解析失败: " << e.what() << "\n";
        std::cerr << "原始响应:\n" << res->body << "\n";
        return 1;
    }

    std::cout << "\n=== 完成 ===\n";






	// 新增的：循环打印多个城市的天气
















    return 0;
}
