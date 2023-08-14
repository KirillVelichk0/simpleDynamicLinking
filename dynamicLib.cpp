#include "dynamicLib.h"
#include <any>

#define LinuxDynamicLib

#ifdef LinuxDynamicLib

#include <dlfcn.h>

struct SharedLibController::SharedLibControllerImpl
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


SharedLibController::SharedLibController(const std::string &path) : impl(std::make_unique<SharedLibControllerImpl>())
{
    this->impl->CreateImpl(path);
}
SharedLibController::~SharedLibController()
{
    this->impl->DestroyImpl();
}
bool SharedLibController::isActive() const
{
    return this->impl->isActive();
}

void* SharedLibController::GetRawFuncCaller(const std::string& funcName){
    return (void*)(dlsym(this->impl->sharedLib, funcName.c_str()));
}



#endif