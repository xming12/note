#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <regex>
#include <string>
#include <vector>
#include "base/log.h"
#include "httplib.h"
#include <fmt/core.h>
#include <fmt/printf.h>
using std::string;
using std::vector;
typedef std::map<string, string> MapStrStr;
typedef uint8_t _BYTE;
string model;

vector<string> globVector(const string& pattern)
{
    glob_t glob_result;
    glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    vector<string> files;
    for (unsigned int i = 0; i < glob_result.gl_pathc; ++i)
    {
        files.push_back(string(glob_result.gl_pathv[i]));
    }
    globfree(&glob_result);
    return files;
}

std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool is_file_exist(const char* fileName)
{
    try
    {
        std::ifstream infile(fileName);
        return infile.good();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return false;
}

bool get_thermals(MapStrStr& thermal_map)
{
    char buffer[100];
    auto len           = strlen("/sys/class/thermal/thermal_zone");
    auto thermal_zones = globVector("/sys/class/thermal/thermal_zone*");
    for (auto& e : thermal_zones)
    {
        auto id = e.substr(len);
        memset(buffer, 0, sizeof(buffer));
        string thermal_zone_type = e + "/type";
        FILE* f_temp             = fopen(thermal_zone_type.c_str(), "rt");
        auto n                   = fread(buffer, 1, sizeof(buffer), f_temp);
        fclose(f_temp);

        string thermal_name(buffer, n - 1);
        thermal_map["thermal_" + thermal_name] = e;
    }
    return true;
}

bool get_kgsl(MapStrStr& kgsl_map)
{
    char buffer[100];

    if (is_file_exist("/sys/class/kgsl/kgsl-3d0/temp"))
    {
        auto len        = strlen("/sys/class/kgsl/kgsl-3d0/");
        auto kgsl_files = globVector("/sys/class/kgsl/kgsl-3d0/*");
        for (auto& e : kgsl_files)
        {
            auto f_name                = e.substr(len);
            kgsl_map["kgsl_" + f_name] = e;
        }
    }
    return true;
}

bool get_power(MapStrStr& power_map)
{
    char buffer[100];

    if (is_file_exist("/sys/class/power_supply/battery"))
    {
        auto len         = strlen("/sys/class/power_supply/battery/");
        auto power_files = globVector("/sys/class/power_supply/battery/*");
        for (auto& e : power_files)
        {
            auto f_name                  = e.substr(len);
            power_map["power_" + f_name] = e;
        }
    }
    return true;
}

string get_vol_current(string s)
{
    auto lines = split(s, '\n');
    string voltage, current;
    for (auto& e : lines)
    {
        auto kv = split(e, '=');
        if (kv[0] == "POWER_SUPPLY_CURRENT_NOW")
        {
            current = kv[1];
        }
        else if (kv[0] == "POWER_SUPPLY_VOLTAGE_NOW")
        {
            voltage = kv[1];
        }
    }    
    std::string res = fmt::sprintf(R"([%s,%s])", voltage, current);    
    return res;
}

int main(int argc, char** argv)
{
    using namespace httplib;
    Server svr;

    //char buffer[2048];

    //char* end;

    char man[PROP_VALUE_MAX + 1], mod[PROP_VALUE_MAX + 1];
    __system_property_get("ro.product.model", mod);
    std::cout << "model:" << mod << std::endl;
    model = mod;

    MapStrStr dev_map;

    // get all thermal
    get_thermals(dev_map);
    // get all kgsl
    get_kgsl(dev_map);
    // get power
    get_power(dev_map);

    for (const auto& [key, value] : dev_map)
    {
        std::cout << key << "=" << value << std::endl;
    }

    // temp list
    float temp = 0;

    svr.Get("/", [&](const httplib::Request&, httplib::Response& res) {
        string data = "gpu_temp yhnu<yiluoyang@kingsoft.com>";
        res.set_content(data, "text/plain");
    });

    svr.Get("/key", [&dev_map](const httplib::Request&, httplib::Response& res) {
        string data = "[";
        for (const auto& [key, value] : dev_map)
        {
            data.append("\"");
            data.append(key);
            data.append("\"");
            data.append(",");
        }
        data.pop_back();
        data.append("]");
        res.set_content(data, "text/plain");
    });

    svr.Get("/test", [&dev_map](const httplib::Request&, httplib::Response& res) {
        string data;
        for (const auto& [key, value] : dev_map)
        {
            data.append(key);
            data.append("=");
            data.append(value);
            data.append("\n");
        }
        for (const auto& [key, value] : dev_map)
        {
            data.append(key);
            data.append("=");
            data.append(value);
            data.append("\n");
        }
        res.set_content(data, "text/plain");
    });

    svr.Get("/kgsl/(.*)", [&dev_map](const httplib::Request& req, httplib::Response& res) {
        char buffer[2048];
        auto datas      = split(req.matches[1], ',');
        string res_data = "[";
        std::regex regexp("[_0-9]+");
        std::smatch m;
        for (const auto& e : datas)
        {
            string dev_path;
            size_t n = 0;
            if (e.rfind("kgsl_") == 0 || e.rfind("power_") == 0)
            {
                dev_path = dev_map[e];
            }
            else
            {
                dev_path = dev_map[e] + "/temp";
            }
            //std::cout << dev_path << std::endl;
            FILE* f_temp = fopen(dev_path.c_str(), "rt");
            if (f_temp)
            {
                n = fread(buffer, 1, sizeof(buffer), f_temp);
                fclose(f_temp);
            }
            string s = string(buffer, n - 1);
            while (regex_search(s, m, regexp))
            {
                auto e = m[0];
                res_data.append(e);
                res_data.append(",");
                s = m.suffix();
            }
        }
        res_data.pop_back();
        res_data.append("]");
        std::cout << res_data << std::endl;;
        res.set_content(res_data, "text/plain");
    });

    svr.Get(R"(/get/(.*))", [&dev_map](const httplib::Request& req, httplib::Response& res) {
        char buffer[2048];
        auto datas      = split(req.matches[1], ',');
        string res_data = "[";
        for (const auto& e : datas)
        {
            string dev_path;
            size_t n = 0;
            if (e.rfind("kgsl_") == 0 || e.rfind("power_") == 0)
            {
                dev_path = dev_map[e];
            }
            else
            {
                dev_path = dev_map[e] + "/temp";
            }
            try
            {
                // std::cout << dev_path << std::endl;
                FILE* f_temp = fopen(dev_path.c_str(), "rt");
                if (f_temp)
                {
                    n = fread(buffer, 1, sizeof(buffer), f_temp);
                    fclose(f_temp);
                }
            }
            catch (const std::exception& e)
            {
                std::cout << e.what() << '\n';
            }
            if (e.rfind("power_uevent") == 0)
            {
                auto vol_current = get_vol_current(string(buffer, n - 1));
                res_data.append(vol_current);
            }
            else
            {
                res_data.append(string(buffer, n - 1));
            }
            res_data.append(",");
        }
        // temp = strtol(buffer, &end, 10) / 1000.0;
        res_data.pop_back();
        res_data.append("]");
        //std::cout << res_data << std::endl;
        res.set_content(res_data, "text/plain");
    });

    svr.Get(R"(/one9/(.*))", [&dev_map](const httplib::Request& req, httplib::Response& res) {
        char buffer[2048];
        auto datas      = split(req.matches[1], ',');
        string res_data = "[";
        std::regex regexp("[0-9]+");
        std::smatch m;
        for (const auto& e : datas)
        {
            string dev_path;
            size_t n = 0;
            if (e.rfind("kgsl_") == 0 || e.rfind("power_") == 0)
            {
                dev_path = dev_map[e];
            }
            else
            {
                dev_path = dev_map[e] + "/temp";
            }
            // std::cout << dev_path << std::endl;
            FILE* f_temp = fopen(dev_path.c_str(), "rt");
            if (f_temp)
            {
                n = fread(buffer, 1, sizeof(buffer), f_temp);
                fclose(f_temp);
            }
            string s = string(buffer, n - 1);
            string jdata;
            if (e.rfind("power_uevent") == 0)
            {
                jdata = get_vol_current(s);                
            }
            else
            {              
                jdata.append("[");
                while (regex_search(s, m, regexp))
                {
                    auto e = m[0];
                    jdata.append(e);
                    jdata.append(",");
                    s = m.suffix();
                }
                jdata.pop_back();
                jdata.append("]");
            }
            res_data.append(fmt::sprintf(R"({"k": "%s", "v": %s},)",e, jdata));
        }        
        res_data.pop_back();
        res_data.append("]");
        //std::cout << res_data << std::endl;;
        res.set_content(res_data, "text/plain");
    });

    //_BYTE *v5; // r5
    svr.Get(R"(/pluginPower/(.*))", [&dev_map](const httplib::Request& req, httplib::Response& res) {        
        auto v10 = (_BYTE *)operator new(0x40u);        
        memcpy(v10, "/sys/class/power_supply/battery/batt_power_meter", 48);
        v10[48] = 0;
        std::cout << v10 << std::endl;
    });
    LOGI("start on port 8080");
    svr.listen("0.0.0.0", 8080);
    /*
    while (true)
    {
        FILE * temp = fopen("/sys/class/thermal/thermal_zone15/temp","rt");
        int numread = fread(buffer,10,1,temp);
        fclose(temp);
        float gpu_temp = strtol(buffer,&end,10)/1000.0;

        temp = fopen("/sys/class/thermal/thermal_zone94/temp","rt");
        numread = fread(buffer,10,1,temp);
        fclose(temp);
        float battery_temp = strtol(buffer,&end,10)/1000.0;

        LOGI("Temp = %5.2fC %5.2fC", gpu_temp, battery_temp);
        sleep(1);
    }
    */
    return 0;
}