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

}