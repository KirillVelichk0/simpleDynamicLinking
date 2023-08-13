#include <iostream>
#include <any>
#include <memory>
#include <type_traits>
#include <functional>
#define LinuxDynamicLib

#ifdef LinuxDynamicLib

#include <dlfcn.h>

// нужно написать обертку, которая позволяет добавить правильное уничтожение указателей
// в том случае, если мы не угадали с типом

#define GENERATE_EXTERN2(method_name, aType, a, bType, b)                                         \
    extern "C"                                                                    \
    {                                                                             \
        void *method_name##externC(aType a, bType b)                                   \
        {                                                                         \
            static_assert(std::is_pointer_v<decltype(method_name(a, b))>); \
            std::any *preRes = new std::any(method_name(a, b));            \
            void *result = reinterpret_cast<void *>(preRes);                      \
            return result;                                                        \
        }                                                                         \
    }

#define GENERATE_EXTERN_VOID2(method_name, aType, a, bType, b)                         \
    extern "C"                                                         \
    {                                                                  \
        void *method_name##externC(aType a, bType b)                                   \
        {                                                              \
            static_assert(std::is_pointer_v<decltype(method_name(a, b))>); \
            std::any *preRes = new std::any(method_name());            \
            void *result = reinterpret_cast<void *>(preRes);           \
            return result;                                             \
        }                                                              \
    }

// уничтожаться должно крайне осторожно
class SharedLibController
{
private:
    void *sharedLib;
    std::string path;

public:
    SharedLibController(const std::string &path) : path(path)
    {
        sharedLib = dlopen(path.c_str(), RTLD_NOW);
    }
    ~SharedLibController()
    {
        if (this->isActive())
        {
            dlclose(this->sharedLib);
        }
    }
    bool isActive() const
    {
        return sharedLib != nullptr;
    }
    template <class ResultType, class... T>
    std::unique_ptr<ResultType> CallFuncFromName(const std::string &funcName, T... args)
    {
        std::unique_ptr<ResultType> result;
        static_assert(!std::is_pointer_v<ResultType>, "It is add pointer mod auto. Please set not pointer type");
        {
            if (this->isActive())
            {
            	std::cout << "isActive\n";
                std::any *preResult;
                auto creater = (void *(*)(T...))dlsym(this->sharedLib, funcName.c_str());
                if (creater != nullptr)
                {
                    preResult = reinterpret_cast<std::any *>(creater(args...));
                    if (preResult->has_value())
                    {
                    	std::cout << "preRes\n";
                        try
                        {
                            result = std::unique_ptr<ResultType>(std::any_cast<ResultType*>(*preResult));
                            std::cout << "goodCast\n";
                        }
                        catch (std::bad_any_cast &_)
                        {
                        }
                    }
                    //delete preResult;
                }
            }
        }
        return result;
    }
};

#endif
