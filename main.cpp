#include "include/sfl/sfl.hpp"

using namespace sfl;

struct Test : BaseStrategy
{
    bool bought = false;

    Test()
    {
        principal = 1000.0;
    }

    bool direction = 0; // 0 for down, 1 for up

    double last_price;

    uint32_t index = 0;

    // Simple strategy that buys if we're at a bottom and sells a stock if it makes over 10%
    void step() override
    {
        if (!index) { index++; return; }

        for (const auto& c : current_stop.points)
        { 
            auto company = Company::get(c.first);
            if (company->name == "Microsoft Corporation")
            {   
                if (last_price < c.second.price && !direction)
                {
                    direction = 1;
                    if (buy(c.first))
                        std::cout << "Bought Microsoft on " << sfl::stringify(current_stop.time, "%b %e, %Y") << " for $" << c.second.price << "\n";
                }
                else if (last_price > c.second.price && direction)
                {
                    direction = 0;
                    std::vector<double> profits(owned.size());
                    for (uint32_t i = 0; i < profits.size(); i++)
                        profits[i] = (owned[i].current_value - owned[i].bought.price) / owned[i].bought.price;

                    const auto it = std::max_element(profits.begin(), profits.end());
                    if (*it > 0.1)
                        if (sell(owned.begin() + std::distance(profits.begin(), it)))
                            std::cout << "Sold Microsoft on " << sfl::stringify(current_stop.time, "%b %e, %Y") << " for $" << c.second.price << "\n";
                }

                last_price = c.second.price;
            }
        }

        const auto value = [&](const Stop& current_stop) -> double
        {
            double v = principal;
            for (const auto& s : owned)
                v += s.current_value;
            return v;
        };

        const auto perc_change = (value(current_stop) - 1000.0) / 1000.0 * 100.0;
        std::cout << sfl::stringify(current_stop.time, "%b %e, %Y %r") << " - " << "Portfolio value: $" << value(current_stop) << " " << (perc_change > 0 ? "+" : "-") << "%" << perc_change << "\n";

        index++;
    }
};

int main()
{
    //addCompany("MSFT", 2023);

    Driver<Test> driver(DATABASE_DIR "/2023.sft");
    driver.run();
}

#if 0
int main6()
{
    File file;
    file.load("aapl.sft");

    for (const auto& id : file.datapoints[file.companies[0]])
    {
        auto d = Datapoint::get(id);
        
        char str[1024];
        time_t time = d->time;
        const tm* t = std::gmtime(&time);
        std::strftime(str, 1024, "%F", t);
        std::cout << str << " " << d->last << "\n";
    }
}

int main3()
{
    const std::string ticker = "AAPL";

    // Get company info
    const auto company = getCompany(ticker);

    File file;

    const auto exchange_name = company["stock_exchange"]["acronym"];
    auto exchange = file.newExchange(exchange_name);
    exchange->city    = company["stock_exchange"]["city"];
    exchange->country = company["stock_exchange"]["country"];

    auto _company = file.newCompany(company["name"], exchange_name);
    _company->ticker = company["symbol"];

    const auto start_date = "2023-01-01";
    const auto end_date   = "2023-12-31";

    uint32_t offset = 0;
    auto count = std::numeric_limits<uint32_t>::max();

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

    file.write("aapl.sft");

    return 0;
}

int main2()
{
    File file;

    /*
    auto e = file.newExchange("New York");
    e->name = "New York";

    auto c = file.newCompany("Microsoft", "New York");
    c->name = "Microsoft";

    auto d = file.newDatapoint("Microsoft");
    d->open = 1.f;

    file.write("test.sft");*/
    file.load("test.sft");
    auto d = Datapoint::get(file.datapoints[file.companies[0]][0]);
    std::cout << Company::get(d->companyID())->name << "\n";
    std::cout << d->open << "\n";
}
#endif