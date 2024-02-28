#pragma once

#include <sfl/def.hpp>

#include "Objects.hpp"

/*

The file type has the following structure:
 
<---- HEADER ---->
uint16_t    version    -- version of file
std::size_t start_date -- first date
std::size_t end_date   -- final date
uint16_t    exchanges  -- amount of exchanges
{
    uint16_t byte_size
    uint8_t name_length
    char*   name
    uint8_t country_length
    char*   country
    uint8_t city_length
    char*   city
} exchange_data[exchanges] -- description of each exchange

uint32_t    companies  -- the amount of companies
{
    uint16_t byte_size
    uint8_t name_length
    char*   name
    uint8_t ticker_length
    char*   ticker
    index_type exchange;
} company_data[companies] -- description of each exchange

{
    std::size_t data_count;
    {
        double open, high, low, last, close, volume;
        std::size_t time;
        index_type company;
    } datapoints[data_count]
} data_sets[companies]

*/


namespace sfl
{

namespace detail
{
template<typename T>
char* write_data(char* it, const T& val)
{
    std::memcpy(it, &val, sizeof(T));
    it += sizeof(T);
    return it;
}

template<>
char* write_data(char* it, const std::string& val)
{
    uint8_t size = val.size();
    it = write_data(it, size);
    std::memcpy(it, val.c_str(), size);
    return it + size;
}

template<typename T>
T read_data(std::ifstream& file)
{
    T v;
    file.read(reinterpret_cast<char*>(&v), sizeof(T));
    return v;
}

template<>
std::string read_data(std::ifstream& file)
{
    std::string v;
    const auto length = read_data<uint8_t>(file);
    v.resize(length);
    file.read(v.data(), length);
    return v;
}

template<typename T>
char* read_data(char* it, T& val)
{
    std::memcpy(&val, it, sizeof(T));
    return it + sizeof(T);
}

char* read_data(char* it, std::string& str)
{
    uint8_t length;
    it = read_data<uint8_t>(it, length);
    str.resize(length);
    std::memcpy(str.data(), it, length);
    return it + length;
}

using DataPointer = std::unique_ptr<char, void(*)(char*)>;

DataPointer getRegion(std::size_t size)
{
    return DataPointer(
        reinterpret_cast<char*>(
            std::calloc(size, 1)
        ),
        [](char* ptr) { std::free(reinterpret_cast<void*>(ptr)); }
    );
}

std::pair<DataPointer, std::size_t> 
serialize(const std::shared_ptr<Exchange>& value)
{
    const std::size_t size = sizeof(uint8_t) * 3 + value->name.size() + value->country.size() + value->city.size();
    auto ptr = getRegion(size);
    char* it = ptr.get();
    it = write_data(it, value->name);
    it = write_data(it, value->country);
    it = write_data(it, value->city);
    return std::pair(std::move(ptr), size);
}

std::shared_ptr<Exchange>
deExchange(std::ifstream& file)
{
    std::string name, country, city;
    read_data<uint16_t>(file);
    name    = read_data<std::string>(file);
    country = read_data<std::string>(file);
    city    = read_data<std::string>(file);

    auto d = Exchange::makeNamed(name);
    d->name    = name;
    d->country = country;
    d->city    = city;
    return d;
}

std::pair<DataPointer, std::size_t> 
serialize(const std::shared_ptr<Company>& value, std::function<index_type(util::id_t)> get_exchange_index)
{
    const std::size_t size = sizeof(uint8_t) * 2 + value->name.size() + value->ticker.size() + sizeof(index_type);
    auto ptr = getRegion(size);
    char* it = ptr.get();
    it = write_data(it, value->name);
    it = write_data(it, value->ticker);
    it = write_data(it, get_exchange_index(value->exchangeID()));
    return std::pair(std::move(ptr), size);
}

std::shared_ptr<Company>
deCompany(std::ifstream& file, std::function<util::id_t(index_type)> get_exchange_id)
{
    std::string name, ticker;
    index_type exchange;

    read_data<uint16_t>(file);
    name     = read_data<std::string>(file);
    ticker   = read_data<std::string>(file);
    exchange = read_data<index_type>(file);

    auto d = Company::makeNamed(name, get_exchange_id(exchange));
    d->name   = name;
    d->ticker = ticker;
    return d;
}

std::pair<DataPointer, std::size_t> 
serialize(const std::shared_ptr<Datapoint>& value, std::function<index_type(util::id_t)> get_company_index)
{
    const std::size_t size = Datapoint::byte_size;
    auto ptr = getRegion(size);
    char* it = ptr.get();
    std::memcpy(it, &value->open, sizeof(double) * 6);
    it += sizeof(double) * 6;
    it = write_data(it, value->time);
    it = write_data(it, get_company_index(value->companyID()));
    return std::pair(std::move(ptr), size);
}

std::shared_ptr<Datapoint>
deDatapoint(std::ifstream& file, std::function<util::id_t(index_type)> get_company_id)
{
    double double_data[6];
    std::size_t time;
    index_type company;

    file.read(reinterpret_cast<char*>(double_data), sizeof(double) * 6);
    time    = read_data<std::size_t>(file);
    company = read_data<index_type>(file);

    auto d = Datapoint::make(get_company_id(company));
    std::memcpy(&d->open, double_data, sizeof(double) * 6);
    d->time = time;
    return d;
}
} // namespace detail

struct File
{
    std::vector<util::id_t> companies, exchanges;
    std::unordered_map<util::id_t, std::vector<util::id_t>> datapoints; // company_id, datapoint

    std::shared_ptr<Exchange>
    newExchange(const std::string& name)
    {
        auto d = Exchange::makeNamed(name);
        d->name = name;
        exchanges.push_back(d->getID());
        return d;
    }

    std::shared_ptr<Company>
    newCompany(const std::string& name, const std::string& exchange)
    {
        auto d = Company::makeNamed(name, util::Universe::getID(exchange));
        d->name = name;
        companies.push_back(d->getID());
        return d;
    }

    std::shared_ptr<Datapoint>
    newDatapoint(const std::string& company)
    {
        const auto company_id = util::Universe::getID(company);
        auto d = Datapoint::make(company_id);
        datapoints[company_id].push_back(d->getID());
        return d;
    }

    void load(const std::string& filename)
    {
        using namespace detail;

        std::ifstream f(filename, std::ios_base::in | std::ios_base::binary);
        assert(f);

        const auto version = read_data<uint16_t>(f);
        assert(version == FILE_VERSION);

        read_data<std::size_t>(f); // smallest_date (maybe don't need)
        read_data<std::size_t>(f); // largest_date (maybe don't need)

        const auto exchanges_size = read_data<uint16_t>(f);
        exchanges.reserve(exchanges_size);
        for (uint16_t i = 0; i < exchanges_size; i++)
            exchanges.push_back(deExchange(f)->getID());

        const auto get_exchange_id = 
        [&](index_type index) -> util::id_t
        {
            assert(index < exchanges.size());
            return exchanges[index];
        };

        const auto companies_size = read_data<uint16_t>(f);
        companies.reserve(companies_size);
        for (uint16_t i = 0; i < companies_size; i++)
            companies.push_back(deCompany(f, get_exchange_id)->getID());
        
        const auto get_company_id =
        [&](index_type index) -> util::id_t
        {
            assert(index < companies.size());
            return companies[index];
        };

        for (uint16_t i = 0; i < companies_size; i++)
        {
            const auto count = read_data<std::size_t>(f);
            datapoints[companies[i]].reserve(count);
            for (std::size_t j = 0; j < count; j++)
                datapoints[companies[i]].push_back(deDatapoint(f, get_company_id)->getID());
        }
    }

    void write(const std::string& filename)
    {
        using namespace detail;

        // sort companies and exchanges by name
        const auto pred = [](auto a, auto b) { return util::Universe::getName(a) < util::Universe::getName(b); };
        std::sort(companies.begin(), companies.end(), pred);
        std::sort(exchanges.begin(), exchanges.end(), pred);

        // we want to sort data points by time as well as grab all the used names
        auto smallest = std::numeric_limits<std::size_t>::max();
        auto largest  = std::numeric_limits<std::size_t>::min();
        for (auto& p : datapoints)
        {
            std::sort(p.second.begin(), p.second.end(), [](auto a, auto b)
            {
                return Datapoint::get(a)->time < Datapoint::get(b)->time;
            });

            if (p.second.size())
            {
                auto first = Datapoint::get(p.second[0]);
                auto last = Datapoint::get(p.second[p.second.size() - 1]);

                if (first->time < smallest) smallest = first->time;
                if (last->time > largest)   largest  = last->time;
            }
        }

        std::vector<util::id_t> used_exchanges;
        std::vector<util::id_t> used_companies;
        for (const auto& c : companies)
        {
            if ([&]()
            {
                for (const auto& p : datapoints)
                    if (p.first == c)
                        return true;
                return false;
            }()) used_companies.push_back(c);

            if ([&]()
            {
                const auto it = std::find(exchanges.begin(), exchanges.end(), Company::get(c)->exchangeID());
                return (it != exchanges.end());
            }()) used_exchanges.push_back(Company::get(c)->exchangeID());
        }

        const auto get_company_index = [&](util::id_t id)
        {
            const auto it = std::find(used_companies.begin(), used_companies.end(), id);
            assert(it != used_companies.end());
            return std::distance(used_companies.begin(), it);
        };

        const auto get_exchange_index = [&](util::id_t id)
        {
            const auto it = std::find(used_exchanges.begin(), used_exchanges.end(), id);
            assert(it != used_exchanges.end());
            return std::distance(used_exchanges.begin(), it);
        };

        // write all the data to the file
        std::ofstream f(filename, std::ios_base::out | std::ios_base::binary);
        assert(f);

        write_value(f, static_cast<uint16_t>(FILE_VERSION));
        write_value(f, smallest);
        write_value(f, largest);

        write_value(f, static_cast<uint16_t>(used_exchanges.size()));
        for (const auto& id : used_exchanges)
        {
            auto data = serialize(Exchange::get(id));
            write_value(f, static_cast<uint16_t>(data.second));
            f.write(data.first.get(), data.second);
        }

        std::size_t total_byte_size = 0;
        write_value(f, static_cast<uint16_t>(used_companies.size()));
        for (const auto& id : used_companies)
        {
            auto data = serialize(Company::get(id), get_exchange_index);
            write_value(f, static_cast<uint16_t>(data.second));
            f.write(data.first.get(), data.second);

            total_byte_size += datapoints[id].size() * Datapoint::byte_size;
        }

        uint32_t it = 0;
        std::vector<char> total_data(total_byte_size + sizeof(std::size_t) * datapoints.size());
        for (const auto& c : used_companies)
        {
            const auto& points = datapoints[c];
            
            std::size_t count = points.size();
            std::memcpy(&total_data[it], &count, sizeof(std::size_t));
            it += sizeof(std::size_t);

            for (const auto& p : points)
            {
                auto data = serialize(Datapoint::get(p), get_company_index);
                std::memcpy(&total_data[it], data.first.get(), data.second);
                it += data.second;
            }
        }

        f.write(total_data.data(), total_data.size());
    }

private:
    template<typename T>
    void write_value(std::ofstream& f, const T& value)
    {
        f.write(reinterpret_cast<const char*>(&value), sizeof(T));
    }
};

} // namespace sfl