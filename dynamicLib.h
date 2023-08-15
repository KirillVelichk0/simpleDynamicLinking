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


class SharedLibController : public std::enable_shared_from_this<SharedLibController>{
private:
    struct SharedLibControllerImpl;
    std::unique_ptr<SharedLibControllerImpl> impl;
    void* GetRawFuncCaller(const std::string& funcName);
    SharedLibController(const std::string &path);
public:
    ~SharedLibController();
    SharedLibController(const SharedLibController& another) = delete;
    SharedLibController(SharedLibController&& another) = delete;
    SharedLibController& operator=(const SharedLibController& another) = delete;
    SharedLibController& operator=(SharedLibController&& another) = delete;
    bool isActive() const;
    std::shared_ptr<SharedLibController> get_ptr(){
        return this->shared_from_this();
    }

    [[nodiscard]]
    std::shared_ptr<SharedLibController> CreateController(const std::string& path){
        return std::shared_ptr<SharedLibController>(new SharedLibController(path));
    }


    //возвращает unique_ptr с результируемым типом и классом удаления в шаблоне
    //лямбда подхватывает экземпляр shared_ptr класса контроллера библиотеки, освобождая его только
    //при удалении используемого объекта. Это гаранитирует, что библиотека будет освобождена не раньше,
    //чем будут удалены все экспортированные в нее объекты
    template <class ResultType>
    auto CallFuncFromNameSafe(const std::string &funcName)
    {
        static_assert(!std::is_pointer_v<ResultType>,
         "It is add pointer mod auto. Please set not pointer type");


        auto self = this->get_ptr();
        auto deleter = [self](ResultType* deletable)mutable{
            self = nullptr;
            delete deletable;
        };
        std::unique_ptr<ResultType, decltype(deleter)> result;
        std::string fullFuncName = funcName + "_ExternC_ViaDynamicLib";
        {
            if (this->isActive())
            {
                std::any *preResult;
                auto creater = (void *(*)(void))(this->GetRawFuncCaller(fullFuncName));
                if (creater != nullptr)
                {
                    void* rawImported = creater();
                    if(rawImported == nullptr){
                        return result;
                    }
                    preResult = reinterpret_cast<std::any *>(rawImported);
                    if (preResult->has_value())
                    {
                        try
                        {
                            auto packedPointer = std::any_cast<PackedPointer<ResultType*>>(*preResult);
                            result = std::unique_ptr<ResultType>(packedPointer.unpack(), deleter);
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
