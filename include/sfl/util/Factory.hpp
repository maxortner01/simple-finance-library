#pragma once

#include <assert.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <type_traits>

namespace util
{
    using id_t = uint32_t;

    struct Universe
    {
        template<typename T, typename... Args>
        inline static std::shared_ptr<T>
        make(Args&&... args)
        {
            const auto type_id = typeid(T).hash_code();
            const auto id = counter++;

            if (!objects.count(type_id)) objects[type_id];
            auto& type_objects = objects.at(type_id);

            assert(!type_objects.count(id));
            auto* obj = new T(id, std::forward<Args>(args)...);
            type_objects.insert(std::pair(
                id,
                std::shared_ptr<void>(
                    reinterpret_cast<void*>(obj),
                    [](void* ptr)
                    { delete reinterpret_cast<T*>(ptr); }
                )
            ));

            return std::reinterpret_pointer_cast<T>(type_objects.at(id));
        }

        template<typename T, typename... Args>
        inline static std::shared_ptr<T>
        makeNamed(const std::string& name, Args&&... args)
        {
            auto r = make<T>(std::forward<Args>(args)...);
            names.insert(std::pair(name, r->getID()));
            return r;
        }

        template<typename T>
        inline static std::shared_ptr<T>
        get(const id_t& id)
        {
            const auto type_id = typeid(T).hash_code();
            assert(objects.count(type_id));
            const auto& type_objects = objects.at(type_id);
            assert(type_objects.count(id));
            return std::reinterpret_pointer_cast<T>(type_objects.at(id));
        }

        template<typename T>
        inline static std::shared_ptr<T>
        get(const std::string& name)
        {
            assert(names.count(name));
            return get<T>(names.at(name));
        }

        inline static id_t 
        getID(const std::string& name)
        {
            assert(names.count(name));
            return names.at(name);
        }

        inline static std::string
        getName(const id_t& id)
        {
            for (const auto& p : names)
                if (p.second == id)
                    return p.first;
            assert(false);
        }

        inline static bool
        exists(const std::string& name)
        {
            return names.count(name);
        }

        inline static void
        destroy(const id_t& id)
        {
            bool found = false;
            for (auto& types : objects)
                for (const auto& objs : types.second)
                    if (id == objs.first)
                    {
                        objects.at(types.first).erase(id);
                        found = true;
                        break;
                    }
            
            assert(found);
        }

    private:
        inline static id_t counter = 1;
        inline static std::unordered_map<std::size_t, std::unordered_map<id_t, std::shared_ptr<void>>> objects;
        inline static std::unordered_map<std::string, id_t> names;
    };

    template<typename Type>
    struct Factory
    {
        Factory(id_t _id) :
            id(_id)
        {   }

        const id_t& getID() const { return id; }

        Factory(Factory&&) = delete;
        Factory(const Factory&) = delete;
        virtual ~Factory() = default;

        /* Factory Handling */
        template<typename... Args>
        static std::shared_ptr<Type>
        make(Args&&... args)
        {
            return Universe::make<Type>(std::forward<Args>(args)...);
        }

        template<typename... Args>
        static std::shared_ptr<Type>
        makeNamed(const std::string& name, Args&&... args)
        {
            return Universe::makeNamed<Type>(name, std::forward<Args>(args)...);
        }

        static std::shared_ptr<Type>
        get(const id_t& id)
        {
            return Universe::get<Type>(id);
        }

        static std::shared_ptr<Type>
        get(const std::string& name)
        {
            return Universe::get<Type>(name);
        }

        static void
        destroy(const id_t& id)
        {
            Universe::destroy(id);
        }

        static void
        destroy(const std::shared_ptr<Type>& obj)
        {
            Universe::destroy(obj->getID());
        }
        
    private:
        id_t id;
    };
}