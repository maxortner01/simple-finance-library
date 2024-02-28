#pragma once

#include <sfl/def.hpp>

#include <nlohmann/json.hpp>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <SL/Lua.hpp>

#include "File.hpp"

#ifndef SOURCE_DIR
#define SOURCE_DIR ""
#endif

#ifndef DATABASE_DIR
#define DATABASE_DIR
#endif

namespace sfl
{

namespace detail
{

static std::string
getAPIKey()
{
    static std::optional<std::string> key;

    if (!key)
    {
        SL::Runtime runtime(SOURCE_DIR "/config.lua");
        assert(runtime);

        const auto& api = runtime.getGlobal<SL::Table>("API");
        key = api->get<SL::String>("key");
    }

    return *key;
}

std::string
getResponse(const std::string& url)
{
    using namespace curlpp::options;

    std::ostringstream ss;
    try
	{
        curlpp::Cleanup myCleanup;
        ss << curlpp::options::Url(url);
	}

	catch(curlpp::RuntimeError & e)
	{
		std::cout << e.what() << std::endl;
	}

	catch(curlpp::LogicError & e)
	{
		std::cout << e.what() << std::endl;
	}
    return ss.str();
}



}

nlohmann::json 
getIntraday(
    const std::string& ticker, 
    const std::string& interval, 
    const std::string& from_date, 
    const std::string& to_date,
    const std::string& offset)
{
    const auto url = [&]() -> std::string
    {
        std::stringstream ss;
        ss << "https://api.marketstack.com/v1/intraday?access_key=";
        ss << detail::getAPIKey();
        ss << "&symbols="   << ticker;
        ss << "&interval="  << interval;
        ss << "&date_from=" << from_date;
        ss << "&date_to="   << to_date;
        ss << "&limit=1000";
        ss << "&offset=" << offset;
        return ss.str();
    }();

    return nlohmann::json::parse(detail::getResponse(url));
}

nlohmann::json
getCompany(
    const std::string& ticker)
{
    const auto url = [&]() -> std::string
    {
        std::stringstream ss;
        ss << "https://api.marketstack.com/v1/tickers/";
        ss << ticker;
        ss << "?access_key=" << detail::getAPIKey();
        
        return ss.str();
    }();

    return nlohmann::json::parse(detail::getResponse(url));
}

void
addCompany(
    const std::string& ticker,
    uint16_t year)
{
    File file;
    const std::string filename = std::string(DATABASE_DIR) + "/" + std::to_string(static_cast<unsigned int>(year)) + ".sft";
    {
        std::ifstream f(
            filename, 
            std::ios_base::in | std::ios_base::binary
        );

        f.close();
        if (f) file.load(filename);
    }

    for (const auto& c : file.companies)
    {
        auto d = Company::get(c);
        if (d->ticker == ticker) return;
    }

    const auto write_file = [&]()
    {
        // Get company info
        const auto company = getCompany(ticker);

        const auto exchange_name = company["stock_exchange"]["acronym"];
        if (!util::Universe::exists(exchange_name))
        {
            auto exchange = file.newExchange(exchange_name);
            exchange->city    = company["stock_exchange"]["city"];
            exchange->country = company["stock_exchange"]["country"];
        }

        auto _company = file.newCompany(company["name"], exchange_name);
        _company->ticker = company["symbol"];

        const auto start_date = (std::stringstream() << year << "-01-01").str();
        const auto end_date   = (std::stringstream() << year << "-12-31").str();

        uint32_t offset = 0;
        bool done = false;
        while (!done)
        {
            const auto page = getIntraday(
                _company->ticker,
                "30min",
                start_date,
                end_date,
                std::to_string(offset)
            );

            const auto& data = page["data"];
            const auto retreived = page["pagination"]["count"].get<uint32_t>();

            if (retreived != 1000) done = true;

            for (uint32_t j = 0; j < retreived; j++)
            {
                if (
                    !data[j]["date"].is_string() ||
                    !data[j]["open"].is_number() ||
                    !data[j]["close"].is_number() ||
                    !data[j]["high"].is_number() ||
                    !data[j]["low"].is_number() ||
                    !data[j]["last"].is_number() 
                ) continue;

                auto d = file.newDatapoint(_company->name);
                d->open  = data[j]["open"].get<double>();
                d->close = data[j]["close"].get<double>();
                d->high  = data[j]["high"].get<double>();
                d->low   = data[j]["low"].get<double>();
                d->last  = data[j]["last"].get<double>();

                const auto date = data[j]["date"].get<std::string>();
                const auto year = std::stoi(date.substr(0, 4));
                const auto month = std::stoi(date.substr(5, 2));
                const auto day = std::stoi(date.substr(8, 2));
                const auto hour = std::stoi(date.substr(11, 2));
                const auto minute = std::stoi(date.substr(14, 2));
                tm _tm{0};
                _tm.tm_min = minute;
                _tm.tm_hour = hour;
                _tm.tm_mday = day;
                _tm.tm_mon = month - 1;
                _tm.tm_year = year - 1900;
                time_t time = mktime(&_tm);
                d->time = static_cast<std::size_t>(time);
            }

            offset += retreived;
        }

        file.write(filename);
    };

    try
    {
        write_file();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

}