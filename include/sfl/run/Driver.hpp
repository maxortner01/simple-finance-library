#pragma once

#include <set>

#include <sfl/def.hpp>
#include <sfl/data/Objects.hpp>

#include <sfl/util/Time.hpp>

namespace sfl
{

struct Timepoint
{
    time_t time;
    double price;
    //util::id_t datapoint;
};

struct Stop
{
    std::size_t time;
    std::unordered_map<util::id_t, Timepoint> points;
    //std::vector<Timepoint> points;
};  

struct Portfolio
{
    struct Event
    {
        enum Type
        {
            Buy, Sell
        } type;
        Timepoint timepoint;
        util::id_t company;
    };

    double principal;
    std::unordered_map<util::id_t, uint32_t> owned_stocks;
    std::vector<Event> events;

    double profit(const Stop& current_stop) const
    {
        double v = 0.0;
        for (const auto& e : events)
            v += (current_stop.points.at(e.company).price - e.timepoint.price) * (e.type == Event::Buy ? 1.0 : -1.0);
        return v;
    }

    double value(const Stop& current_stop) const
    {
        double v = principal;
        for (const auto& e : events)
            v += e.timepoint.price * (e.type == Event::Buy ? 1.0 : -1.0);
        return v + profit(current_stop);
    }
};

struct Stock
{
    util::id_t company;
    double current_value;
    Timepoint bought;
};

struct BaseStrategy
{
    //Portfolio portfolio;
    double principal;
    std::vector<Stock> owned;
    std::span<const Stop> history;
    Stop current_stop;

    bool buy(const util::id_t& company_id)
    {
        if (!current_stop.points.count(company_id))
            return false;

        const auto& timepoint = current_stop.points[company_id];
        if (principal < timepoint.price)
            return false;

        principal -= timepoint.price;
        owned.push_back(Stock{
            .bought = timepoint,
            .company = company_id,
            .current_value = timepoint.price
        });

        return true;
    }

    bool sell(std::vector<Stock>::iterator stock)
    {
        principal += stock->current_value;
        owned.erase(stock);
        return true;
    }

    /*
    bool buy(const util::id_t& company_id)
    {
        if (!current_stop.points.count(company_id) ||
             portfolio.principal < current_stop.points[company_id].price)
                return false;

        portfolio.owned_stocks[company_id]++;
        portfolio.principal -= current_stop.points[company_id].price;
        portfolio.events.push_back(Portfolio::Event {
            .type = Portfolio::Event::Buy,
            .timepoint = current_stop.points[company_id],
            .company = company_id
        });

        return true;
    }

    bool sell(const util::id_t& company_id)
    {
        if (!current_stop.points.count(company_id) ||
            !portfolio.owned_stocks.count(company_id) ||
            !portfolio.owned_stocks[company_id] ||
            portfolio.principal < current_stop.points[company_id].price)
            return false;

        portfolio.owned_stocks[company_id]--;
        portfolio.principal += current_stop.points[company_id].price;
        portfolio.events.push_back(Portfolio::Event {
            .type = Portfolio::Event::Sell,
            .timepoint = current_stop.points[company_id],
            .company = company_id
        });

        return true;
    }*/

    virtual void start() {}
    virtual void stop()  {}

    virtual void step() = 0;
};

template<class T, class U>
concept Derived = std::is_base_of<U, T>::value;

template<typename T>
concept Strategy = Derived<T, BaseStrategy>;

template<Strategy S>
struct Driver
{
    template<typename... Args>
    Driver(const std::string& filename, Args&&... args)
    {
        strategy = std::make_unique<S>(std::forward<Args>(args)...);
        file.load(filename);

        for (const auto& p : file.datapoints)
            timeseries[p.first].resize(p.second.size());
    }

    void run()
    {
        // time series will (in general) not have the same amount of points
        // so we need to take the one with the largest amount (as this is the finest grain resolution)

        // find the latest first time and the earliest last time
        std::size_t lastest_time  = std::numeric_limits<std::size_t>::min();
        std::size_t earliest_time = std::numeric_limits<std::size_t>::max();
        for (const auto& p : file.datapoints)
        {
            const auto earliest = Datapoint::get(p.second.front())->time;
            const auto latest = Datapoint::get(p.second.back())->time;

            if (earliest > lastest_time ) lastest_time  = earliest;
            if (latest   < earliest_time) earliest_time = latest;
        }

        // gather all of the time points
        using id_index_pair = std::pair<util::id_t, std::size_t>;

        std::vector<id_index_pair> times;
        for (const auto& p : timeseries)
        {
            std::size_t index = 0;
            for (const auto& id : file.datapoints[p.first])
            {
                auto d = Datapoint::get(id);
                if (d->time >= lastest_time && d->time <= earliest_time)
                    times.push_back(std::pair(id, index));

                index++;
            }
        }

        // sort it by time
        std::sort(times.begin(), times.end(), [](auto a, auto b) { return Datapoint::get(a.first)->time < Datapoint::get(b.first)->time; });

        auto current_time = Datapoint::get(times[0].first)->time;
        std::vector<std::vector<id_index_pair>> groups(1);
        
        std::vector<id_index_pair>* current = &groups[0];

        uint32_t index = 1;
        for (const auto& time : times)
        {
            auto d = Datapoint::get(time.first);
            if (d->time > current_time) 
            {
                current_time = d->time;
                groups.push_back(std::vector<id_index_pair>());
                current = &groups[index++];
            }

            current->push_back(time);
        }

        std::vector<Stop> stops(groups.size());

        std::size_t i = 0;
        for (const auto& g : groups)
        {
            const auto g_time = Datapoint::get(g[0].first)->time;
            stops[i].time = g_time;

            const auto left_over = [&]() -> std::set<util::id_t>
            {
                std::set<util::id_t> companies;
                for (const auto& c : file.companies)
                    companies.insert(c);

                for (const auto& p : g)
                    companies.erase(Datapoint::get(p.first)->companyID());
                
                return companies;
            }();

            for (const auto& p : g)
            {
                auto d = Datapoint::get(p.first);
                stops[i].points.insert(std::pair(
                    d->companyID(),
                    Timepoint {
                        .price     = (d->open + d->close) / 2.0,
                        .time      = static_cast<time_t>(d->time)
                    }
                ));
            }

            for (const auto& c : left_over)
            {
                std::optional<util::id_t> a, b;

                if (i <= 1)
                {
                    // grab the next index value
                    // must be >= 1
                    int64_t index = -1;
                    std::size_t start = i + 1;
                    while (index == -1)
                    {
                        for (const auto& p : groups[start])
                            if (Datapoint::get(p.first)->companyID() == c) // if this group has the desired company
                            {
                                index = p.second;
                                assert(index >= 1);
                                assert(file.datapoints.count(c));
                                assert(index < file.datapoints[c].size());
                                b = file.datapoints[c][index];
                                a = file.datapoints[c][index - 1];
                                break;
                            }
                        
                        assert(start < groups.size());
                        start++;
                    }
                }
                else
                {
                    // grab the last value
                    int64_t index = -1;
                    std::size_t start = i - 1;
                    while (index == -1)
                    {
                        for (const auto& p : groups[start])
                            if (Datapoint::get(p.first)->companyID() == c) // if this group has the desired company
                            {
                                index = p.second;
                                assert(file.datapoints.count(c));
                                assert(index + 1 < file.datapoints[c].size());
                                a = file.datapoints[c][index];
                                b = file.datapoints[c][index + 1];
                                break;
                            }
                        
                        assert(start >= 0);
                        start--;
                    }
                }

                assert(a && b);

                auto d1 = Datapoint::get(*a);
                auto d2 = Datapoint::get(*b);

                const auto time_a = d1->time;
                const auto time_b = d2->time;

                const auto t = (double)(g_time - time_a) / (double)(time_b - time_a); 
                
                const auto price_a = (d1->open + d1->close) / 2.0;
                const auto price_b = (d2->open + d2->close) / 2.0;

                stops[i].points.insert(
                    std::pair(
                        d1->companyID(),
                        Timepoint {
                            .price = price_a * (1 - t) + t * price_b, // interpolation function
                            .time  = static_cast<time_t>(g_time)
                        }
                    )
                );

            }

            i++;
        }

        // Go through each group and find the missing company and interpolate value
        for (uint32_t i = 0; i < stops.size(); i++)
        {
            strategy->history = std::span<const Stop>(stops.begin(), i);
            strategy->current_stop = stops[i];

            for (Stock& s : strategy->owned)
                s.current_value = stops[i].points[s.company].price;

            strategy->step();
        }
    }

private:
    std::unordered_map<util::id_t, std::vector<Timepoint>> timeseries; // company, list

    File file;
    std::unique_ptr<S> strategy;
};

}
