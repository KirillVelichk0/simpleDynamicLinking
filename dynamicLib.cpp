#include "dynamicLib.h"

#define LinuxDynamicLib

#ifdef LinuxDynamicLib

#include <dlfcn.h>

struct DynamicLibController::DynamicLibControllerImpl
{
    void *sharedLib;
    std::string path;
    void CreateImpl(const std::string &path)
    {
        this->path = path;
        this->sharedLib = dlopen(path.c_str(), RTLD_NOW);
    }
    void DestroyImpl()
    {
        if (this->isActive())
        {
            dlclose(this->sharedLib);
            this->sharedLib = nullptr;
        }
    }
    bool isActive() const
    {
        return this->sharedLib != nullptr;
    }

    
};


DynamicLibController::DynamicLibController(const std::string &path) : impl(std::make_unique<DynamicLibControllerImpl>())
{
    this->impl->CreateImpl(path);
}
DynamicLibController::~DynamicLibController()
{
    this->impl->DestroyImpl();
}
bool DynamicLibController::isActive() const
{
    return this->impl->isActive();
}

void* DynamicLibController::GetRawFuncCaller(const std::string& funcName){
    return (void*)(dlsym(this->impl->sharedLib, funcName.c_str()));
}



#endif
