#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <type_traits>

class ArgsParser
{
public:
    ArgsParser(int argc, char* argv[])
    {
        m_CmdName = argv[0];
        for (int i = 1; i < argc; ++i)
            Parse(argv[i]);
    }

    bool Has(std::string_view key) const
    {
        return m_Args.contains(std::string(key));
    }

    template<typename T>
    T Get(std::string_view key, T defValue = {}) const
    {
        auto it = m_Args.find(std::string(key));
        if (it == m_Args.end())
            return defValue;
        return Convert<T>(it->second);
    }

    const char *GetCmdName()
    {
        return m_CmdName.c_str();
    }

private:
    void Parse(std::string_view arg)
    {
        auto pos = arg.find('=');
        if (pos == std::string_view::npos)
        {
            m_Args[std::string(arg)] = "true";
            return;
        }

        m_Args[std::string(arg.substr(0, pos))] = std::string(arg.substr(pos + 1));
    }

    template<typename T>
    T Convert(const std::string& str) const
    {
        if constexpr (std::is_same_v<T, std::string>)
            return str;
        else if constexpr (std::is_same_v<T, int32_t>)
            return std::stoi(str);
        else if constexpr (std::is_same_v<T, int64_t>)
            return std::stoll(str);
        else if constexpr (std::is_same_v<T, double>)
            return std::stod(str);
        else if constexpr (std::is_same_v<T, bool>)
            return str == "1" || str == "true" || str == "yes" || str == "on";
        else {
            static_assert(!sizeof(T), "Unsupported type");
        }
    }

    std::string m_CmdName;
    std::unordered_map<std::string, std::string> m_Args;
};
