#pragma once

#include <sfl/def.hpp>

namespace sfl
{
    
struct Exchange : util::Factory<Exchange>
{
    Exchange(util::id_t id) :
        Factory(id)
    {   }

    std::string name, country, city;
};

struct Company : util::Factory<Company>
{
    Company(util::id_t id, util::id_t e) :
        Factory(id),
        exchange(e)
    {   }

    std::string name, ticker;

    const util::id_t& exchangeID() const { return exchange; }

private:
    util::id_t exchange;
};

struct Datapoint : util::Factory<Datapoint>
{
    Datapoint(util::id_t id, util::id_t c) :
        Factory(id),
        company(c)
    {   }

    double open, high, low, last, close, volume;
    std::size_t time;

    const util::id_t& companyID() const { return company; }

    constexpr static std::size_t byte_size = sizeof(double) * 6 + sizeof(std::size_t) + sizeof(index_type);

private:
    util::id_t company;
};

}