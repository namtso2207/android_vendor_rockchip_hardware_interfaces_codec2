/*
 * Copyright (C) 2020 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "C2RKStore"
// #define LOG_NDEBUG 0
#include <utils/Log.h>

#include <C2Component.h>
#include <C2ComponentFactory.h>
#include <C2DmaBufAllocator.h>
#include <util/C2InterfaceHelper.h>
#include <dlfcn.h>
#include <map>
#include <mutex>

#include "C2RKPlatformSupport.h"
#include "mpp_soc.h"

namespace android {

#define C2_RK_COMPONENT_PATH        "libcodec2_rk_component.so"

static bool system_secure_supported(void) {
    static int result = -1;

    if (result == -1) {
        struct stat buffer;
        result = (stat("/dev/dma_heap/secure", &buffer) == 0);
    }
    return (result == 1);
};

class C2RKComponentStore : public C2ComponentStore {
public:
    virtual std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override;
    virtual std::shared_ptr<C2ParamReflector> getParamReflector() const override;
    virtual C2String getName() const override;
    virtual c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery> &fields) const override;
    virtual c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>> *const params) const override;
    virtual c2_status_t query_sm(
            const std::vector<C2Param*> &stackParams,
            const std::vector<C2Param::Index> &heapParamIndices,
            std::vector<std::unique_ptr<C2Param>> *const heapParams) const override;
    virtual c2_status_t createInterface(
            C2String name, std::shared_ptr<C2ComponentInterface> *const interface) override;
    virtual c2_status_t createComponent(
            C2String name, std::shared_ptr<C2Component> *const component) override;
    virtual c2_status_t copyBuffer(
            std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) override;
    virtual c2_status_t config_sm(
            const std::vector<C2Param*> &params,
            std::vector<std::unique_ptr<C2SettingResult>> *const failures) override;

    C2RKComponentStore();

    virtual ~C2RKComponentStore() override = default;

private:

    /**
     * An object encapsulating a loaded component module.
     *
     * \todo provide a way to add traits to known components here to avoid loading the .so-s
     * for listComponents
     */
    struct ComponentModule : public C2ComponentFactory,
            public std::enable_shared_from_this<ComponentModule> {
        virtual c2_status_t createComponent(
                c2_node_id_t id, std::shared_ptr<C2Component> *component,
                ComponentDeleter deleter = std::default_delete<C2Component>()) override;
        virtual c2_status_t createInterface(
                c2_node_id_t id, std::shared_ptr<C2ComponentInterface> *interface,
                InterfaceDeleter deleter = std::default_delete<C2ComponentInterface>()) override;

        /**
         * \returns the traits of the component in this module.
         */
        std::shared_ptr<const C2Component::Traits> getTraits();

        /**
         * Creates an uninitialized component module.
         *
         * \param name[in]  component name.
         *
         * \note Only used by ComponentLoader.
         */
        ComponentModule()
            : mInit(C2_NO_INIT),
              mLibHandle(nullptr),
              createFactory(nullptr),
              destroyFactory(nullptr),
              mComponentFactory(nullptr) {
        }

        /**
         * Initializes a component module with a given library path. Must be called exactly once.
         *
         * \note Only used by ComponentLoader.
         *
         * \param libPath[in] library path
         *
         * \retval C2_OK        the component module has been successfully loaded
         * \retval C2_NO_MEMORY not enough memory to loading the component module
         * \retval C2_NOT_FOUND could not locate the component module
         * \retval C2_CORRUPTED the component module could not be loaded (unexpected)
         * \retval C2_REFUSED   permission denied to load the component module (unexpected)
         * \retval C2_TIMED_OUT could not load the module within the time limit (unexpected)
         */
        c2_status_t init(std::string componentName);

        virtual ~ComponentModule() override;

        typedef ::C2ComponentFactory* (*CreateRKCodec2FactoryFunc)(std::string componentName);
        typedef void (*DestroyRKCodec2FactoryFunc)(::C2ComponentFactory*);

    protected:
        std::recursive_mutex mLock; ///< lock protecting mTraits
        std::shared_ptr<C2Component::Traits> mTraits; ///< cached component traits

        c2_status_t mInit; ///< initialization result

        void *mLibHandle; ///< loaded library handle
        CreateRKCodec2FactoryFunc createFactory; ///< loaded create function
        DestroyRKCodec2FactoryFunc destroyFactory; ///< loaded destroy function
        C2ComponentFactory *mComponentFactory; ///< loaded/created component factory
    };

    /**
     * An object encapsulating a loadable component module.
     *
     * \todo make this also work for enumerations
     */
    struct ComponentLoader {
        /**
         * Load the component module.
         *
         * This method simply returns the component module if it is already currently loaded, or
         * attempts to load it if it is not.
         *
         * \param module[out] pointer to the shared pointer where the loaded module shall be stored.
         *                    This will be nullptr on error.
         *
         * \retval C2_OK        the component module has been successfully loaded
         * \retval C2_NO_MEMORY not enough memory to loading the component module
         * \retval C2_NOT_FOUND could not locate the component module
         * \retval C2_CORRUPTED the component module could not be loaded
         * \retval C2_REFUSED   permission denied to load the component module
         */
        c2_status_t fetchModule(std::shared_ptr<ComponentModule> *module) {
            c2_status_t res = C2_OK;
            std::lock_guard<std::mutex> lock(mMutex);
            std::shared_ptr<ComponentModule> localModule = mModule.lock();
            if (localModule == nullptr) {
                localModule = std::make_shared<ComponentModule>();
                res = localModule->init(mComponentName);
                if (res == C2_OK) {
                    mModule = localModule;
                }
            }
            *module = localModule;
            return res;
        }

        /**
         * Creates a component loader for a specific library path (or name).
         */
        ComponentLoader(std::string compoenentName)
            : mComponentName(compoenentName) {}

    private:
        std::mutex mMutex; ///< mutex guarding the module
        std::weak_ptr<ComponentModule> mModule; ///< weak reference to the loaded module
        std::string mComponentName; ///< library path
    };

    struct Interface : public C2InterfaceHelper {
        std::shared_ptr<C2StoreIonUsageInfo> mIonUsageInfo;
        std::shared_ptr<C2StoreDmaBufUsageInfo> mDmaBufUsageInfo;

        Interface(std::shared_ptr<C2ReflectorHelper> reflector)
            : C2InterfaceHelper(reflector) {
            setDerivedInstance(this);

            struct Setter {
                static C2R setIonUsage(bool /* mayBlock */, C2P<C2StoreIonUsageInfo> &me) {
                    me.set().heapMask = ~0;
                    me.set().allocFlags = 0;
                    me.set().minAlignment = 0;
                    return C2R::Ok();
                }

                static C2R setDmaBufUsage(bool /* mayBlock */, C2P<C2StoreDmaBufUsageInfo> &me) {
                    long long usage = (long long)me.get().m.usage;
                    if ((usage & C2MemoryUsage::READ_PROTECTED) && system_secure_supported()) {
                        strncpy(me.set().m.heapName, "secure", me.v.flexCount());
                    } else if (C2DmaBufAllocator::system_uncached_supported() &&
                        !(usage & (C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE))) {
                        strncpy(me.set().m.heapName, "system-uncached", me.v.flexCount());
                    } else {
                        strncpy(me.set().m.heapName, "system", me.v.flexCount());
                    }
                    me.set().m.allocFlags = 0;
                    return C2R::Ok();
                };
            };

            addParameter(
                DefineParam(mIonUsageInfo, "ion-usage")
                .withDefault(new C2StoreIonUsageInfo())
                .withFields({
                    C2F(mIonUsageInfo, usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mIonUsageInfo, capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mIonUsageInfo, heapMask).any(),
                    C2F(mIonUsageInfo, allocFlags).flags({}),
                    C2F(mIonUsageInfo, minAlignment).equalTo(0)
                })
                .withSetter(Setter::setIonUsage)
                .build());

            addParameter(
                DefineParam(mDmaBufUsageInfo, "dmabuf-usage")
                .withDefault(C2StoreDmaBufUsageInfo::AllocShared(0))
                .withFields({
                    C2F(mDmaBufUsageInfo, m.usage).flags({C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE}),
                    C2F(mDmaBufUsageInfo, m.capacity).inRange(0, UINT32_MAX, 1024),
                    C2F(mDmaBufUsageInfo, m.allocFlags).flags({}),
                    C2F(mDmaBufUsageInfo, m.heapName).any(),
                })
                .withSetter(Setter::setDmaBufUsage)
                .build());
        }
    };

    /**
     * Retrieves the component module for a component.
     *
     * \param module pointer to a shared_pointer where the component module will be stored on
     *               success.
     *
     * \retval C2_OK        the component loader has been successfully retrieved
     * \retval C2_NO_MEMORY not enough memory to locate the component loader
     * \retval C2_NOT_FOUND could not locate the component to be loaded
     * \retval C2_CORRUPTED the component loader could not be identified due to some modules being
     *                      corrupted (this can happen if the name does not refer to an already
     *                      identified component but some components could not be loaded due to
     *                      bad library)
     * \retval C2_REFUSED   permission denied to find the component loader for the named component
     *                      (this can happen if the name does not refer to an already identified
     *                      component but some components could not be loaded due to lack of
     *                      permissions)
     */
    c2_status_t findComponent(C2String name, std::shared_ptr<ComponentModule> *module);

    /**
     * Loads each component module and discover its contents.
     */
    void visitComponents();

    std::mutex mMutex; ///< mutex guarding the component lists during construction
    bool mVisited; ///< component modules visited
    std::map<C2String, ComponentLoader> mComponents; ///< componentName -> component module
    std::vector<std::shared_ptr<const C2Component::Traits>> mComponentList;

    std::shared_ptr<C2ReflectorHelper> mReflector;
    Interface mInterface;
};

c2_status_t C2RKComponentStore::ComponentModule::init(std::string componentName) {
    mLibHandle = dlopen(C2_RK_COMPONENT_PATH, RTLD_NOW | RTLD_NODELETE);
    LOG_ALWAYS_FATAL_IF(mLibHandle == nullptr,
            "could not dlopen %s: %s", C2_RK_COMPONENT_PATH, dlerror());

    createFactory =
        (CreateRKCodec2FactoryFunc)dlsym(mLibHandle, "CreateRKCodec2Factory");
    LOG_ALWAYS_FATAL_IF(createFactory == nullptr,
            "createFactory is null in %s", C2_RK_COMPONENT_PATH);

    destroyFactory =
        (DestroyRKCodec2FactoryFunc)dlsym(mLibHandle, "DestroyRKCodec2Factory");
    LOG_ALWAYS_FATAL_IF(destroyFactory == nullptr,
            "destroyFactory is null in %s", C2_RK_COMPONENT_PATH);

    mComponentFactory = createFactory(componentName);
    if (mComponentFactory == nullptr) {
        ALOGD("could not create factory in %s", C2_RK_COMPONENT_PATH);
        mInit = C2_NO_MEMORY;
    } else {
        mInit = C2_OK;
    }

    if (mInit != C2_OK) {
        return mInit;
    }

    std::shared_ptr<C2ComponentInterface> intf;
    c2_status_t res = createInterface(0, &intf);
    if (res != C2_OK) {
        ALOGD("failed to create interface: %d", res);
        return mInit;
    }

    std::shared_ptr<C2Component::Traits> traits(new (std::nothrow) C2Component::Traits);
    if (traits) {
        traits->name = intf->getName();

        C2ComponentKindSetting kind;
        C2ComponentDomainSetting domain;
        res = intf->query_vb({ &kind, &domain }, {}, C2_MAY_BLOCK, nullptr);
        bool fixDomain = res != C2_OK;
        if (res == C2_OK) {
            traits->kind = kind.value;
            traits->domain = domain.value;
        } else {
            // TODO: remove this fall-back
            ALOGD("failed to query interface for kind and domain: %d", res);

            traits->kind =
                (traits->name.find("encoder") != std::string::npos) ? C2Component::KIND_ENCODER :
                (traits->name.find("decoder") != std::string::npos) ? C2Component::KIND_DECODER :
                C2Component::KIND_OTHER;
        }

        uint32_t mediaTypeIndex =
                traits->kind == C2Component::KIND_ENCODER ? C2PortMediaTypeSetting::output::PARAM_TYPE
                : C2PortMediaTypeSetting::input::PARAM_TYPE;
        std::vector<std::unique_ptr<C2Param>> params;
        res = intf->query_vb({}, { mediaTypeIndex }, C2_MAY_BLOCK, &params);
        if (res != C2_OK) {
            ALOGD("failed to query interface: %d", res);
            return mInit;
        }
        if (params.size() != 1u) {
            ALOGD("failed to query interface: unexpected vector size: %zu", params.size());
            return mInit;
        }
        C2PortMediaTypeSetting *mediaTypeConfig = C2PortMediaTypeSetting::From(params[0].get());
        if (mediaTypeConfig == nullptr) {
            ALOGD("failed to query media type");
            return mInit;
        }
        traits->mediaType =
            std::string(mediaTypeConfig->m.value,
                        strnlen(mediaTypeConfig->m.value, mediaTypeConfig->flexCount()));

        if (fixDomain) {
            if (strncmp(traits->mediaType.c_str(), "audio/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_AUDIO;
            } else if (strncmp(traits->mediaType.c_str(), "video/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_VIDEO;
            } else if (strncmp(traits->mediaType.c_str(), "image/", 6) == 0) {
                traits->domain = C2Component::DOMAIN_IMAGE;
            } else {
                traits->domain = C2Component::DOMAIN_OTHER;
            }
        }

        // TODO: get this properly from the store during emplace
        switch (traits->domain) {
        case C2Component::DOMAIN_AUDIO:
            traits->rank = 8;
            break;
        default:
            traits->rank = 128;
        }

        params.clear();
        res = intf->query_vb({}, { C2ComponentAliasesSetting::PARAM_TYPE }, C2_MAY_BLOCK, &params);
        if (res == C2_OK && params.size() == 1u) {
            C2ComponentAliasesSetting *aliasesSetting =
                C2ComponentAliasesSetting::From(params[0].get());
            if (aliasesSetting) {
                // Split aliases on ','
                // This looks simpler in plain C and even std::string would still make a copy.
                char *aliases = ::strndup(aliasesSetting->m.value, aliasesSetting->flexCount());
                ALOGD("'%s' has aliases: '%s'", intf->getName().c_str(), aliases);

                for (char *tok, *ptr, *str = aliases; (tok = ::strtok_r(str, ",", &ptr));
                        str = nullptr) {
                    traits->aliases.push_back(tok);
                    ALOGD("adding alias: '%s'", tok);
                }
                free(aliases);
            }
        }
    }
    mTraits = traits;

    return mInit;
}

C2RKComponentStore::ComponentModule::~ComponentModule() {
    if (destroyFactory && mComponentFactory) {
        destroyFactory(mComponentFactory);
    }
    if (mLibHandle) {
        ALOGV("unloading dll");
        dlclose(mLibHandle);
    }
}

c2_status_t C2RKComponentStore::ComponentModule::createInterface(
        c2_node_id_t id, std::shared_ptr<C2ComponentInterface> *interface,
        std::function<void(::C2ComponentInterface*)> deleter) {
    interface->reset();
    if (mInit != C2_OK) {
        return mInit;
    }
    std::shared_ptr<ComponentModule> module = shared_from_this();
    c2_status_t res = mComponentFactory->createInterface(
            id, interface, [module, deleter](C2ComponentInterface *p) mutable {
                // capture module so that we ensure we still have it while deleting interface
                deleter(p); // delete interface first
                module.reset(); // remove module ref (not technically needed)
    });
    return res;
}

c2_status_t C2RKComponentStore::ComponentModule::createComponent(
        c2_node_id_t id, std::shared_ptr<C2Component> *component,
        std::function<void(::C2Component*)> deleter) {
    component->reset();
    if (mInit != C2_OK) {
        return mInit;
    }
    std::shared_ptr<ComponentModule> module = shared_from_this();
    c2_status_t res = mComponentFactory->createComponent(
            id, component, [module, deleter](C2Component *p) mutable {
                // capture module so that we ensure we still have it while deleting component
                deleter(p); // delete component first
                module.reset(); // remove module ref (not technically needed)
    });
    return res;
}

std::shared_ptr<const C2Component::Traits> C2RKComponentStore::ComponentModule::getTraits() {
    std::unique_lock<std::recursive_mutex> lock(mLock);
    return mTraits;
}

bool isHardwareSupport(C2String name) {
    int32_t coding = GetMppCodingFromComponentName(name);
    int32_t type = GetMppCtxTypeFromComponentName(name);

    if (!mpp_check_soc_cap((MppCtxType)type, (MppCodingType)coding)) {
        return false;
    }
    return true;
}

C2RKComponentStore::C2RKComponentStore()
    : mVisited(false),
      mReflector(std::make_shared<C2ReflectorHelper>()),
      mInterface(mReflector) {
    auto emplace = [this](const char *componentName) {
        mComponents.emplace(componentName, componentName);
    };

   for (int i = 0; i < sComponentMapsSize; ++i) {
        if (isHardwareSupport(sComponentMaps[i].name)) {
            ALOGD("plugin %s", sComponentMaps[i].name.c_str());
            emplace(sComponentMaps[i].name.c_str());
        }
    }
}

c2_status_t C2RKComponentStore::copyBuffer(
        std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) {
    (void)src;
    (void)dst;
    return C2_OMITTED;
}

c2_status_t C2RKComponentStore::query_sm(
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<C2Param>> *const heapParams) const {
    return mInterface.query(stackParams, heapParamIndices, C2_MAY_BLOCK, heapParams);
}

c2_status_t C2RKComponentStore::config_sm(
        const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>> *const failures) {
    return mInterface.config(params, C2_MAY_BLOCK, failures);
}

void C2RKComponentStore::visitComponents() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mVisited) {
        return;
    }
    for (auto &nameAndLoader : mComponents) {
        ComponentLoader &loader = nameAndLoader.second;
        std::shared_ptr<ComponentModule> module;
        if (loader.fetchModule(&module) == C2_OK) {
            std::shared_ptr<const C2Component::Traits> traits = module->getTraits();
            if (traits) {
                mComponentList.push_back(traits);
            }
        }
    }
    mVisited = true;
}

std::vector<std::shared_ptr<const C2Component::Traits>> C2RKComponentStore::listComponents() {
    // This method SHALL return within 500ms.
    visitComponents();
    return mComponentList;
}

c2_status_t C2RKComponentStore::findComponent(
        C2String name, std::shared_ptr<ComponentModule> *module) {
    (*module).reset();
    visitComponents();

    auto pos = mComponents.find(name);
    if (pos != mComponents.end()) {
        return pos->second.fetchModule(module);
    }
    return C2_NOT_FOUND;
}

c2_status_t C2RKComponentStore::createComponent(
        C2String name, std::shared_ptr<C2Component> *const component) {
    // This method SHALL return within 100ms.
    component->reset();
    std::shared_ptr<ComponentModule> module;
    c2_status_t res = findComponent(name, &module);
    if (res == C2_OK) {
        // TODO: get a unique node ID
        res = module->createComponent(0, component);
    }
    return res;
}

c2_status_t C2RKComponentStore::createInterface(
        C2String name, std::shared_ptr<C2ComponentInterface> *const interface) {
    // This method SHALL return within 100ms.
    interface->reset();
    std::shared_ptr<ComponentModule> module;
    c2_status_t res = findComponent(name, &module);
    if (res == C2_OK) {
        // TODO: get a unique node ID
        res = module->createInterface(0, interface);
    }
    return res;
}

c2_status_t C2RKComponentStore::querySupportedParams_nb(
        std::vector<std::shared_ptr<C2ParamDescriptor>> *const params) const {
    return mInterface.querySupportedParams(params);
}

c2_status_t C2RKComponentStore::querySupportedValues_sm(
        std::vector<C2FieldSupportedValuesQuery> &fields) const {
    return mInterface.querySupportedValues(fields, C2_MAY_BLOCK);
}

C2String C2RKComponentStore::getName() const {
    return "android.componentStore.rockchip";
}

std::shared_ptr<C2ParamReflector> C2RKComponentStore::getParamReflector() const {
    return mReflector;
}

std::shared_ptr<C2ComponentStore> GetCodec2RKComponentStore() {
    static std::mutex mutex;
    static std::weak_ptr<C2ComponentStore> platformStore;
    std::lock_guard<std::mutex> lock(mutex);
    std::shared_ptr<C2ComponentStore> store = platformStore.lock();
    if (store == nullptr) {
        store = std::make_shared<C2RKComponentStore>();
        platformStore = store;
    }
    return store;
}

} // namespace android
