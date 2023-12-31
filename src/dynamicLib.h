#include <iostream>
#include <any>
#include <memory>
#include <type_traits>
#include <functional>
#include <string>
#include <optional>

using namespace std::string_literals;


template <class T>
concept NotPointerConcept = !std::is_pointer_v<T>;

 
template <class T>
concept PointerConcept = std::is_pointer_v<T>;

class DynamicLibController;
using SharedLibController = std::shared_ptr<DynamicLibController>;


struct ImportError{
    std::string errorStr;
};

/*!
 * \brief Класс импортированной функции
 * \details Класс импортированной функции.
 * Содержит указатель на вызываемую функцию,
 * а также экземпляр shared_ptr контроллера динамической библиотеки, ассоциированной с этой функции
 */
template <class ResultType, class... ArgsType>
class ImportedFunction{
private:
    ResultType (*importedFunc)(ArgsType...);
    SharedLibController importedLib;
public:
    explicit ImportedFunction(ResultType (*importedFunc)(ArgsType...), SharedLibController imported = nullptr)
    : importedFunc(importedFunc), importedLib(std::move(imported)) {}
    ResultType operator()(ArgsType... args){
        return this->importedFunc(args...);
    }
    auto GetLib(){
        return this->importedLib;
    }
};
/*!
 * \brief Класс импортированного объекта
 * \details Класс импортированного объекта.
 * Содержит указатель на импортированный объект,
 * а также экземпляр shared_ptr контроллера динамической библиотеки, ассоциированной с этой функции.
 * Невозможно создать экземпляр данного класса, зато можно создать shared_ptr с помощью фабрики.
 */
template <NotPointerConcept ImportedObjectType>
class ImportedObject{
private:
    std::shared_ptr<ImportedObjectType> importedObject;
    SharedLibController libController;
    explicit ImportedObject(ImportedObjectType* importedObject, SharedLibController lib = nullptr) : libController(std::move(lib)){
        this->importedObject = std::shared_ptr<ImportedObjectType>(importedObject);
    }
public:
    ~ImportedObject() = default;
    ImportedObject(const ImportedObjectType& another) = delete;
    ImportedObject(ImportedObjectType&& another) = delete;
    ImportedObjectType& operator=(const ImportedObjectType& another) = delete;
    ImportedObjectType& operator=(ImportedObjectType&& another) = delete;
    [[nodiscard]] static std::shared_ptr<ImportedObject> CreateImportedObject(ImportedObjectType* importedObject, SharedLibController lib = nullptr){
        return std::shared_ptr<ImportedObject>(new ImportedObject(importedObject, lib));
    }
    auto GetLib() {
        return this->libController;
    }

    //Не сохранять возвращаемый объект в полях!
    auto GetImported(){
        return this->importedObject;
    }
};


 


template <PointerConcept T>
class PackedPointer
{
private:
    using DelType = std::optional<void (*)(T val)>;
    T val;
    DelType deleter;

public:
    explicit PackedPointer(T val) : val(val) {

        this->deleter = [](T deletableObject) mutable{
            std::cout << "Deleter called" << std::endl;
            delete deletableObject;
        };
    }
    PackedPointer(PackedPointer&& another) {
        this->val = another.val;
        this->deleter = another.deleter;
        another.deleter.reset();
    }
    ~PackedPointer()
    {
        std::cout << "Destructor called" << std::endl;
        if (deleter.has_value())
        {
            deleter.value()(val);
        }
    }
    [[nodiscard]] T Unpack()
    {
        deleter.reset();
        return val;
    }
};

template<class Func>
constexpr bool IsItNonArgsFunc(Func functionName){
    constexpr bool isVoidArgs = requires{functionName();};
    return isVoidArgs;
}

/*! \def GENERATE_SAFE_EXTERN_VOID(function_name)
 * Генерация безопасной обертки для иморта функции
 */
/*!
 * \brief Макрос для импорта порождаеющей функции.
 * \details Макрос для импорта порождаеющей функции.
 * Создает безопасную обертку для функции с указанным именем, которая позволяет безопасно импортировать
 *порождаемый этой функцией объект в сторонние программы.
 * Требования к функции (проверяемые статически):
 * - отсутствие аргументов
 * - результат - указатель, отличный от void*
 * Требования к функции (проверяемые динамически):
 * - не должны выбрасываться исключения. Иначе будет выброшено исключение при подключении динной функции.
 * - возвращаемый указатель должен быть не нулевым
 * Основная идея использования данного макроса:
 * Создание функции, возвращающей указатель на полиморфный объект (с использованием наследования) и обеспечение
 * безопасного импорта с помощью данного макроса.
 * Гарантии безопасности, предоставляемые данным макросом:
 * - Прежде всего, для безопасного импорта используется обертка std::any, поэтому в случае передачи некорректного
 * типа при импорте будет выброшено исключение
 * - с помощью api для безопасного импорта не получится импортировать функцию, не отмеченную данным макросом
 * - так как преобразование типов аргумента не является безопасным в случае неудачи и гарантии правильной передачи
 * нельзя обеспечить, аргументы запрещены
 * - перехватывается некорректное поведение с выбрасыванием исключений и возвратом нулевого указателя
 */
#define GENERATE_SAFE_EXTERN_VOID(function_name)                                  \
    extern "C"                                                               \
    {                                                                        \
        void *function_name##_ExternC_ViaDynamicLib()                        \
        {                                                                       \
            void* result;                                                                \
            constexpr bool isVoidArgs = IsItNonArgsFunc(&function_name);              \
            static_assert(isVoidArgs, "This is not void args in function");       \
            static_assert(!std::is_same_v<std::decay_t<decltype(function_name())>, void*>, "Return type can`t be void*");  \
            static_assert(std::is_pointer_v<decltype(function_name())>, "This function must return a pointer");     \
            try{                                                                    \
                auto pFromFunc = function_name();                        \
                if(pFromFunc != nullptr){                                                           \
                    using ResultType = decltype(PackedPointer(pFromFunc));                              \
                    std::shared_ptr<ResultType> shared_packed_ptr = std::make_shared<ResultType>(PackedPointer(pFromFunc));     \
                    std::any *preRes = new std::any(shared_packed_ptr); \
                    result = reinterpret_cast<void *>(preRes);                 \
                } else{                                                         \
                    ImportError error; error.errorStr = "returned nullptr pointer"s; \
                    std::any* preRes = new std::any(error);\
                    result = reinterpret_cast<void*>(preRes);                   \
                }                                                                  \
            } catch(std::exception& ex){                                                    \
                    ImportError error; error.errorStr = "Catched exception:\n"s + ex.what();           \
                    std::any* preRes = new std::any(error);\
                    result = reinterpret_cast<void*>(preRes);                   \
                } catch(...){                                                     \
                    ImportError error; error.errorStr = "Catched exception:\n"s;           \
                    std::any* preRes = new std::any(error);\
                    result = reinterpret_cast<void*>(preRes); \
                }                                                                               \
                                                                                                        \
                                                                                            \
            return result;                                                   \
        }                                                                    \
    }\

/*!
 * \brief Класс контроллера динамической библиотеки
 * \details Класс контроллера динамической библиотеки.
 * Предоставляет API для безопасного импорта функции, обернутой с помощью макроса GENERATE_SAFE_EXTERN_VOID,
 * а также ручной импорт функции, соблюдающей C-шные требования.
 * Экземпляры данного класса порождаются с помощью фабрики, а также данный класс имеет примесь
 * enable_shared_from_this.
 */
class DynamicLibController : public std::enable_shared_from_this<DynamicLibController>
{
private:
    struct DynamicLibControllerImpl;
    std::unique_ptr<DynamicLibControllerImpl> impl;
    void *GetRawFuncCaller(const std::string &funcName);
    explicit DynamicLibController(const std::string &path);
    std::shared_ptr<DynamicLibController> get_ptr()
    {
        return this->shared_from_this();
    }

    [[nodiscard]] auto GetFuncRawPointerFromNameInternal(const std::string& funcName){
        if (!this->isActive())
        {
            throw std::runtime_error("Controller is not active");
        }
        void *rawFuncPointer = this->GetRawFuncCaller(funcName);
        if (rawFuncPointer == nullptr)
        {
            throw std::runtime_error("Function with name "s + funcName + " does not imported");
        }
        return rawFuncPointer;
    }


    template <NotPointerConcept ResultType>
    [[nodiscard]] auto CallFuncFromNameSafeInternal(const std::string &funcName)
    {
        ResultType* result;
        std::string fullFuncName = funcName + "_ExternC_ViaDynamicLib";
        if (!this->isActive())
        {
            throw std::runtime_error("Controller is not active");
        }
        std::any *preResult;
        void *rawFuncPointer = this->GetRawFuncCaller(fullFuncName);
        if (rawFuncPointer == nullptr)
        {
            throw std::runtime_error("Function with name "s + funcName + " does not imported");
        }
        auto creater = (void *(*)())(rawFuncPointer);
        void *rawImported = creater();
        if (rawImported == nullptr)
        {
            throw std::runtime_error("Function with name "s + funcName + " return nullptr pointer");
        }
        preResult = reinterpret_cast<std::any *>(rawImported);
        if (!preResult->has_value())
        {
            throw std::runtime_error("Something is wrong with imported pointer");
        }
        try
        {
            auto packedPointer = std::any_cast<std::shared_ptr<PackedPointer<ResultType *>>>(*preResult);
            result = packedPointer->Unpack();
        }
        catch (std::bad_any_cast &_)
        {
            try{
                auto err = std::any_cast<ImportError>(*preResult);
                delete preResult;
                throw std::runtime_error(err.errorStr);
            } catch(std::bad_any_cast& _){
                delete preResult;
                throw std::runtime_error("Uncorrect result type");
            }

        }
        delete preResult;
        return result;
    }

public:
    ~DynamicLibController();
    DynamicLibController(const DynamicLibController &another) = delete;
    DynamicLibController(DynamicLibController &&another) = delete;
    DynamicLibController &operator=(const DynamicLibController &another) = delete;
    DynamicLibController &operator=(DynamicLibController &&another) = delete;
    bool isActive() const;

    [[nodiscard]] static std::shared_ptr<DynamicLibController> CreateController(const std::string &path)
    {
        std::shared_ptr<DynamicLibController> result(new DynamicLibController(path));
        if (!result->isActive())
        {
            throw std::runtime_error("Uncorrect path to shared lib. Controller don`t create correctly");
        }
        return result;
    }

    template <class ResultType, class... ArgsTypes>
    [[nodiscard]] ImportedFunction<ResultType, ArgsTypes...> GetFuncFromName(const std::string& funcName){
        try{
            void* rawFuncPointer = this->GetFuncRawPointerFromNameInternal(funcName);
            ResultType (*castedFuncPointer)(ArgsTypes...) = (ResultType(*)(ArgsTypes...))(rawFuncPointer);
            return ImportedFunction<ResultType, ArgsTypes...>(castedFuncPointer, this->get_ptr());
        } catch(std::runtime_error& err){
            throw err;
        }
    }


    template <NotPointerConcept ResultType>
    [[nodiscard]] std::shared_ptr<ImportedObject<ResultType>> CallFuncFromNameSafe(const std::string &funcName)
    {
        auto self = this->get_ptr();
        try{
            ResultType* rawRes = this->CallFuncFromNameSafeInternal<ResultType>(funcName);
            return ImportedObject<ResultType>::CreateImportedObject(rawRes, self);
        } catch(std::runtime_error& e){
            throw e;
        }
    }
};


