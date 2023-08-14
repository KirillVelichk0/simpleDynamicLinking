#include <iostream>
#include <any>
#include <memory>
#include <type_traits>
#include <functional>



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

#define GENERATE_EXTERN_VOID(method_name)                         \
    extern "C"                                                         \
    {                                                                  \
        void *method_name##externC(aType a, bType b)                                   \
        {                                                              \
            static_assert(std::is_pointer_v<decltype(method_name())>); \
            std::any *preRes = new std::any(method_name());            \
            void *result = reinterpret_cast<void *>(preRes);           \
            return result;                                             \
        }                                                              \
    }



class SharedLibController{
private:
    struct SharedLibControllerImpl;
    std::unique_ptr<SharedLibControllerImpl> impl;
    void* GetRawFuncCaller(const std::string& funcName);
public:
    SharedLibController(const std::string &path);
    ~SharedLibController();
    SharedLibController(const SharedLibController& another) = delete;
    SharedLibController(SharedLibController&& another) = delete;
    SharedLibController& operator=(const SharedLibController& another) = delete;
    SharedLibController& operator=(SharedLibController&& another) = delete;
    bool isActive() const;
    template <class ResultType, class... T>
    std::unique_ptr<ResultType> CallFuncFromName(const std::string &funcName, T... args)
    {
        std::unique_ptr<ResultType> result;
        static_assert(!std::is_pointer_v<ResultType>,
         "It is add pointer mod auto. Please set not pointer type");
        {
            if (this->isActive())
            {
                std::cout << "isActive\n";
                std::any *preResult;
                auto creater = (void *(*)(T...))(this->GetRawFuncCaller(funcName));
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
                    delete preResult;
                }
            }
        }
        return result;
    }
};
