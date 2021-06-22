/*
 * --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-cw authors
 * --------------------------------------------------------------------------
 */

#include "clock_manager.h"
#include "file_utils.h"
#include "clocks.h"
#include "process_management.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
#include <filesystem>

int dirExists(const char *path)
{
    struct stat info;

    if(stat( path, &info ) != 0)
        return 0;
    else if(info.st_mode & S_IFDIR)
        return 1;
    else
        return 0;
}

ClockManager* ClockManager::instance = NULL;

ClockManager* ClockManager::GetInstance()
{
    return instance;
}

void ClockManager::Exit()
{
    if(instance)
    {
        delete instance;
    }
}

void ClockManager::Initialize()
{
    if(!instance)
    {
        instance = new ClockManager();
    }
}

ClockManager::ClockManager()
{
    this->config = Config::CreateDefault();
    this->context = new SysClkContext;
    this->context->applicationId = 0;
    this->context->profile = SysClkProfile_Handheld;
    this->context->enabled = false;
    for(unsigned int i = 0; i < SysClkModule_EnumMax; i++)
    {
        this->context->freqs[i] = 0;
        this->context->overrideFreqs[i] = 0;
    }
    this->running = false;
    this->lastTempLogNs = 0;
    this->lastCsvWriteNs = 0;
}

ClockManager::~ClockManager()
{
    delete this->config;
    delete this->context;
}

void ClockManager::SetRunning(bool running)
{
    this->running = running;
}

bool ClockManager::Running()
{
    return this->running;
}

std::string formatTid(uint64_t tid)
{
    char str[17];
    snprintf(str, sizeof(str), "%016lX", tid);
    return std::string(str);
}

void ClockManager::Tick()
{
    std::scoped_lock lock{this->contextMutex};
    this->config->Refresh();
    this->RefreshContext();
    
    // {
    //     std::uint32_t hz = 0;
    //     for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
    //     {
    //         hz = this->context->overrideFreqs[module];

    //         if(!hz)
    //         {
    //             hz = this->config->GetAutoClockHz(this->context->applicationId, (SysClkModule)module, this->context->profile);
    //         }

    //         if (hz)
    //         {
    //             hz = Clocks::GetNearestHz((SysClkModule)module, this->context->profile, hz);

    //             if (hz != this->context->freqs[module] && this->context->enabled)
    //             {
    //                 FileUtils::LogLine("[mgr] %s clock set : %u.%u Mhz", Clocks::GetModuleName((SysClkModule)module, true), hz/1000000, hz/100000 - hz/1000000*10);
    //                 Clocks::SetHz((SysClkModule)module, hz);
    //                 this->context->freqs[module] = hz;
    //             }
    //         }
    //     }
    // }
}

void ClockManager::WaitForNextTick()
{
    svcSleepThread(this->GetConfig()->GetConfigValue(SysClkConfigValue_PollingIntervalMs) * 1000000ULL);
}


void manageClingWrap(uint64_t oldTitleId, uint64_t newTitleId){


    if (formatTid(newTitleId) == "050000BADDAD0000") {
        FileUtils::LogLine("[CLINGWRAP] Target found");

        if (std::filesystem::exists("sdmc:/bootloader/patches.ini")){
            FileUtils::LogLine("[CLINGWRAP] patches exists -> rename");
            std::filesystem::rename("sdmc:/bootloader/patches.ini", "sdmc:/bootloader/_patchesCW.ini");
        }

        // if (dirExists("sdmc:/bootloader")){
        //     FileUtils::LogLine("[CLINGWRAP] bootloader exists -> rename");
        //     std::filesystem::rename("sdmc:/bootloader", "sdmc:/_b0otloader");
        // }
        // if (dirExists("sdmc:/atmosphere/kips")){
        //     FileUtils::LogLine("[CLINGWRAP] atmosphere/kips exists -> rename");
        //     std::filesystem::rename("sdmc:/atmosphere/kips", "sdmc:/atmosphere/_k1ps");
        // }

    } else if (formatTid(oldTitleId) == "050000BADDAD0000") {
        FileUtils::LogLine("[CLINGWRAP] Target exited");

        if (std::filesystem::exists("sdmc:/bootloader/patches.ini")){
            FileUtils::LogLine("[CLINGWRAP] patches already exists -> skipping");
        } else {
            if (std::filesystem::exists("sdmc:/bootloader/_patchesCW.ini")){
                FileUtils::LogLine("[CLINGWRAP] backup patches exists -> renaming to patches");
                std::filesystem::rename("sdmc:/bootloader/_patchesCW.ini", "sdmc:/bootloader/patches.ini");
            } else {
                FileUtils::LogLine("[CLINGWRAP] clingwrap patches does not exist too -> OH NO");
            }
        }


        // if (dirExists("sdmc:/bootloader")){
        //     FileUtils::LogLine("[CLINGWRAP] bootloader already exists -> skipping");
        // } else {
        //     if (dirExists("sdmc:/_b0otloader")){
        //         FileUtils::LogLine("[CLINGWRAP] clingbootloader exists -> renaming to bootloader");
        //         std::filesystem::rename("sdmc:/_b0otloader", "sdmc:/bootloader");
        //     } else {
        //         FileUtils::LogLine("[CLINGWRAP] clingbootloader does not exist -> OH NO");
        //     }
        // }

        // if (dirExists("sdmc:/atmosphere/kips")){
        //     FileUtils::LogLine("[CLINGWRAP] bootloader already exists -> skipping");
        // } else {
        //     if (dirExists("sdmc:/atmosphere/_k1ps")){
        //         FileUtils::LogLine("[CLINGWRAP] clingk1ps exists -> renaming to kips");
        //         std::filesystem::rename("sdmc:/atmosphere/_k1ps", "sdmc:/bootloader");
        //     } else {
        //         FileUtils::LogLine("[CLINGWRAP] clingk1ps does not exist -> OH NO");
        //     }
        // }


    } else {
        // switching from one random tid to some other
    }
}

bool ClockManager::RefreshContext()
{
    bool hasChanged = false;

    bool enabled = this->GetConfig()->Enabled();
    if(enabled != this->context->enabled)
    {
        this->context->enabled = enabled;
        FileUtils::LogLine("[mgr] " TARGET " status: %s", enabled ? "enabled" : "disabled");
        hasChanged = true;
    }

    std::uint64_t applicationId = ProcessManagement::GetCurrentApplicationId();
    if (applicationId != this->context->applicationId)
    {
        manageClingWrap(this->context->applicationId, applicationId);

        // FileUtils::LogLine("[mgr] TitleID change from: %016lX to: %016lX", this->context->applicationId, applicationId);
        this->context->applicationId = applicationId;
        hasChanged = true;
    }

    SysClkProfile profile = Clocks::GetCurrentProfile();
    if (profile != this->context->profile)
    {
        FileUtils::LogLine("[mgr] Profile change: %s", Clocks::GetProfileName(profile, true));
        this->context->profile = profile;
        hasChanged = true;
    }

    // restore clocks to stock values on app or profile change
    if(hasChanged)
    {
        Clocks::ResetToStock();
    }

    std::uint32_t hz = 0;
    for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
    {
        // hz = Clocks::GetCurrentHz((SysClkModule)module);
        // if (hz != 0 && hz != this->context->freqs[module])
        // {
        //     FileUtils::LogLine("[mgr] %s clock change: %u.%u Mhz", Clocks::GetModuleName((SysClkModule)module, true), hz/1000000, hz/100000 - hz/1000000*10);
        //     this->context->freqs[module] = hz;
        //     hasChanged = true;
        // }

        // hz = this->GetConfig()->GetOverrideHz((SysClkModule)module);
        // if (hz != this->context->overrideFreqs[module])
        // {
        //     if(hz)
        //     {
        //         FileUtils::LogLine("[mgr] %s override change: %u.%u Mhz", Clocks::GetModuleName((SysClkModule)module, true), hz/1000000, hz/100000 - hz/1000000*10);
        //     }
        //     else
        //     {
        //         FileUtils::LogLine("[mgr] %s override disabled", Clocks::GetModuleName((SysClkModule)module, true));
        //     }
        //     this->context->overrideFreqs[module] = hz;
        //     hasChanged = true;
        // }
    }

    // temperatures do not and should not force a refresh, hasChanged untouched
    // std::uint32_t millis = 0;
    // std::uint64_t ns = armTicksToNs(armGetSystemTick());
    // std::uint64_t tempLogInterval = this->GetConfig()->GetConfigValue(SysClkConfigValue_TempLogIntervalMs) * 1000000ULL;
    // bool shouldLogTemp = tempLogInterval && ((ns - this->lastTempLogNs) > tempLogInterval);
    // for (unsigned int sensor = 0; sensor < SysClkThermalSensor_EnumMax; sensor++)
    // {
    //     // millis = Clocks::GetTemperatureMilli((SysClkThermalSensor)sensor);
    //     // if(shouldLogTemp)
    //     // {
    //     //     FileUtils::LogLine("[mgr] %s temp: %u.%u °C", Clocks::GetThermalSensorName((SysClkThermalSensor)sensor, true), millis/1000, (millis - millis/1000*1000) / 100);
    //     // }
    //     // this->context->temps[sensor] = millis;
    // }

    // if(shouldLogTemp)
    // {
    //     this->lastTempLogNs = ns;
    // }

    // std::uint64_t csvWriteInterval = this->GetConfig()->GetConfigValue(SysClkConfigValue_CsvWriteIntervalMs) * 1000000ULL;

    // if(csvWriteInterval && ((ns - this->lastCsvWriteNs) > csvWriteInterval))
    // {
    //     FileUtils::WriteContextToCsv(this->context);
    //     this->lastCsvWriteNs = ns;
    // }

    return hasChanged;
}

SysClkContext ClockManager::GetCurrentContext()
{
    std::scoped_lock lock{this->contextMutex};
    return *this->context;
}

Config* ClockManager::GetConfig()
{
    return this->config;
}
