#include <iostream>
#include <any>
#include <memory>
#include <type_traits>
#include <functional>

template <class T>
class PackedPointer{
private:
    T val;
    std::function<void(T)> deleter;
public:
    PackedPointer(const T& val, std::function<void(T)> deleter = nullptr) : val(val), deleter(deleter){
        static_assert(std::is_pointer_v<T>, "It is not pointer type");
    }
    ~PackedPointer(){
        if(deleter != nullptr){
            deleter(val);
        }
    }
    [[nodiscard]]
    T Unpack(){
        deleter = nullptr;
        return val;
    }

};


#define GENERATE_EXTERN_VOID(method_name)                         \
    extern "C"                                                         \
    {                                                                  \
        void *method_name##_ExternC_ViaDynamicLib()                                   \
        {                                                              \
            static_assert(std::is_pointer_v<decltype(method_name())>); \
            std::any *preRes = new std::any(PackedPointer(method_name()));            \
            void *result = reinterpret_cast<void *>(preRes);           \
            return result;                                             \
        }                                                              \
    }


#define GENERATE_EXTERN_VOID_WITH_DELETER(method_name, deleterObj)     \
    extern "C"                                                         \
    {                                                                  \
        void *method_name##_ExternC_ViaDynamicLib()                                   \
        {                                                              \
            static_assert(std::is_pointer_v<decltype(method_name())>); \
            std::any *preRes = new std::any(PackedPointer(method_name(), deleterObj)); \
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
    template <class ResultType>
    std::unique_ptr<ResultType> CallFuncFromName(const std::string &funcName)
    {
        std::unique_ptr<ResultType> result;
        std::string fullFuncName = funcName + "_ExternC_ViaDynamicLib";
        static_assert(!std::is_pointer_v<ResultType>,
         "It is add pointer mod auto. Please set not pointer type");
        {
            if (this->isActive())
            {
                std::cout << "isActive\n";
                std::any *preResult;
                auto creater = (void *(*)(void))(this->GetRawFuncCaller(fullFuncName));
                if (creater != nullptr)
                {
                    preResult = reinterpret_cast<std::any *>(creater());
                    if (preResult->has_value())
                    {
                        std::cout << "preRes\n";
                        try
                        {
                            result = std::unique_ptr<ResultType>(std::any_cast<PackedPointer<ResultType*>>(*preResult));
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
