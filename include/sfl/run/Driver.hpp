#pragma once

#include <sfl/def.hpp>
#include <sfl/data/Objects.hpp>

namespace sfl
{

struct Timepoint
{
    time_t time;
    double price;
    util::id_t datapoint;
};

struct BaseStrategy
{
    virtual void step(
        const std::shared_ptr<Company>& company,
        std::span<const Timepoint> history, 
        const Timepoint& data) = 0;
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
        // should run simultaneous time points at the same time
        for (auto& p : timeseries)
        {
            auto company = Company::get(p.first);
            for (uint32_t i = 0; i < p.second.size(); i++)
            {
                auto d = Datapoint::get( file.datapoints[p.first][i] );
                
                Timepoint t;
                t.datapoint = d->getID();
                t.time = d->time;
                t.price = (d->high + d->low) / 2.0;

                p.second[i] = t;
                strategy->step(company, std::span<const Timepoint>(p.second.begin(), i), t);
            }
        }
    }

private:
    std::unordered_map<util::id_t, std::vector<Timepoint>> timeseries; // company, list

    File file;
    std::unique_ptr<S> strategy;
};

}