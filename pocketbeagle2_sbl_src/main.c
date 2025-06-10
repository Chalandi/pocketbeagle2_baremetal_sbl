/*
 *  Copyright (C) 2018-2024 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/device_manager/sciclient.h>
#include <drivers/bootloader.h>
#include <drivers/rtc.h>

#define BOOTLOADER_SD_CONFIG_FILENAME                   ("config.txt")
#define BOOTLOADER_SD_A53_APPIMAGE_FILENAME             ("app_a53.bin")
#define BOOTLOADER_SD_M4F_APPIMAGE_FILENAME             ("app_m4f.bin")
#define BOOTLOADER_SD_R5F_APPIMAGE_FILENAME             ("app_r5f.bin")

#define BOOTLOADER_SD_MAX_NO_OF_CPUS                   (3)

#define BOOTLOADER_APPIMAGE_MAX_FILE_SIZE (0x800000) /* Size of section DDR specified in linker.cmd */

uint8_t gAppImageBuf[BOOTLOADER_APPIMAGE_MAX_FILE_SIZE] __attribute__((aligned(128), section(".bss.filebuf")));

uint32_t gAppImageSize = 0;

uint8_t socCpuCores = 0;

Bootloader_CpuInfo bootCpuInfo[CSL_CORE_ID_MAX];

uint32_t m4f_start_address=0x05000000; //m4f mcu always boots from IRAM at address 0 which is mapped to 0x05000000 in the R5F memory map
uint32_t r5f_start_address=0x70000000;
uint32_t a53_start_address=0x80000000;

uint32_t m4f_clock_in_Hz=0;
uint32_t r5f_clock_in_Hz=0;
uint32_t a53_clock_in_Hz=0;

char m4f_binary_file[30] = {0};
char r5f_binary_file[30] = {0};
char a53_binary_file[30] = {0};

char* gBootLoaderSDFiles[BOOTLOADER_SD_MAX_NO_OF_CPUS] = {NULL};
char** pBinaryFiles = gBootLoaderSDFiles;

int App_OpenloadableImage(char* imageName);
int32_t App_PowerUpCpu(Bootloader_CpuInfo *cpuInfo);
int32_t App_runCpus(void);
int32_t APP_socOpenSramFirewalls(void);

//-----------------------------------------------------------------------------------------
/// \brief  
///
/// \param  
///
/// \return 
//-----------------------------------------------------------------------------------------

int main()
{
    int32_t status;
    uint32_t appImageSize = 0;
    uint8_t coreIdx = 0;

    strcpy((char*)m4f_binary_file, BOOTLOADER_SD_M4F_APPIMAGE_FILENAME);
    strcpy((char*)r5f_binary_file, BOOTLOADER_SD_R5F_APPIMAGE_FILENAME);
    strcpy((char*)a53_binary_file, BOOTLOADER_SD_A53_APPIMAGE_FILENAME);


    gBootLoaderSDFiles[CSL_CORE_ID_M4FSS0_0] = (char*)m4f_binary_file;
    gBootLoaderSDFiles[CSL_CORE_ID_R5FSS0_0] = (char*)r5f_binary_file;
    gBootLoaderSDFiles[CSL_CORE_ID_A53SS0_0] = (char*)a53_binary_file;

    m4f_clock_in_Hz = Bootloader_socCpuGetClkDefault(CSL_CORE_ID_M4FSS0_0);
    r5f_clock_in_Hz = Bootloader_socCpuGetClkDefault(CSL_CORE_ID_R5FSS0_0);
    a53_clock_in_Hz = Bootloader_socCpuGetClkDefault(CSL_CORE_ID_A53SS0_0);


    Bootloader_profileReset();

    Bootloader_socWaitForFWBoot();
    status = Bootloader_socOpenFirewalls();

    DebugP_assert(status == SystemP_SUCCESS);

    RTC_erratumi2327Init();

    System_init();
    Bootloader_profileAddProfilePoint("System_init");

    Board_init();
    Bootloader_profileAddProfilePoint("Board_init");

    Drivers_open();
    Bootloader_profileAddProfilePoint("Drivers_open");

    DebugP_log("********* SBL Start *********\r\n");

    status = Board_driversOpen();
    DebugP_assert(status == SystemP_SUCCESS);
    Bootloader_profileAddProfilePoint("Board_driversOpen");

    status = Sciclient_getVersionCheck(1);

    while(!DDR_isInitDone())
    {
        ClockP_usleep(100);
    }

    if(SystemP_SUCCESS != APP_socOpenSramFirewalls())
    {
        DebugP_log("[KO] Access to DDR SDRAM (512MB) and PSRAM (16K) is disabled\r\n");
    }

    if(SystemP_SUCCESS == status)
    {
        Bootloader_openDma();

        /* open config.txt if exists */
        if(SystemP_SUCCESS == App_OpenloadableImage(BOOTLOADER_SD_CONFIG_FILENAME))
        {
            char *line = strtok((char*)gAppImageBuf, "\n");
            while(line != NULL) {
                sscanf(line, "a53_start_address=0x%x", &a53_start_address);
                sscanf(line, "m4f_start_address=0x%x", &m4f_start_address);
                sscanf(line, "r5f_start_address=0x%x", &r5f_start_address);
                sscanf(line, "a53_clock_in_Hz=%d", &a53_clock_in_Hz);
                sscanf(line, "m4f_clock_in_Hz=%d", &m4f_clock_in_Hz);
                sscanf(line, "r5f_clock_in_Hz=%d", &r5f_clock_in_Hz);
                sscanf(line, "a53_binary_file=%s", (char*)a53_binary_file);
                sscanf(line, "m4f_binary_file=%s", (char*)m4f_binary_file);
                sscanf(line, "r5f_binary_file=%s", (char*)r5f_binary_file);
                line = strtok(NULL, "\n");
            }
        }
        /* print information on the UART console */
        DebugP_log("a53_binary_file   =%s\r\n", a53_binary_file);
        DebugP_log("m4f_binary_file   =%s\r\n", m4f_binary_file);
        DebugP_log("r5f_binary_file   =%s\r\n", r5f_binary_file);
        DebugP_log("a53_start_address =0x%x\r\n",a53_start_address);
        DebugP_log("m4f_start_address =0x%x\r\n",m4f_start_address);
        DebugP_log("r5f_start_address =0x%x\r\n",r5f_start_address);
        DebugP_log("a53_clock_in_Hz   =%d\r\n",a53_clock_in_Hz);
        DebugP_log("m4f_clock_in_Hz   =%d\r\n",m4f_clock_in_Hz);
        DebugP_log("r5f_clock_in_Hz   =%d\r\n",r5f_clock_in_Hz);

        /* update the boot info */
        bootCpuInfo[CSL_CORE_ID_M4FSS0_0] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_M4FSS0_0, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=m4f_clock_in_Hz, .entryPoint=m4f_start_address, .smpEnable=0};
        bootCpuInfo[CSL_CORE_ID_R5FSS0_0] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_R5FSS0_0, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=r5f_clock_in_Hz, .entryPoint=r5f_start_address, .smpEnable=0};
        bootCpuInfo[CSL_CORE_ID_A53SS0_0] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_A53SS0_0, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=a53_clock_in_Hz, .entryPoint=a53_start_address, .smpEnable=0};
        bootCpuInfo[CSL_CORE_ID_A53SS0_1] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_A53SS0_1, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=a53_clock_in_Hz, .entryPoint=a53_start_address, .smpEnable=0};
        bootCpuInfo[CSL_CORE_ID_A53SS1_0] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_A53SS1_0, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=a53_clock_in_Hz, .entryPoint=a53_start_address, .smpEnable=0};
        bootCpuInfo[CSL_CORE_ID_A53SS1_1] = (Bootloader_CpuInfo){.cpuId=CSL_CORE_ID_A53SS1_1, .rprcOffset= BOOTLOADER_INVALID_ID, .clkHz=a53_clock_in_Hz, .entryPoint=a53_start_address, .smpEnable=0};

        while(coreIdx < BOOTLOADER_SD_MAX_NO_OF_CPUS)
        {
            if(App_OpenloadableImage(pBinaryFiles[coreIdx]) == SystemP_SUCCESS)
            {
                uintptr_t EntryPoint = (uintptr_t)bootCpuInfo[coreIdx].entryPoint;

                /* mark the cpu as bootable */
                socCpuCores |= (1u << coreIdx);

                if(CSL_CORE_ID_A53SS0_0 == coreIdx)
                {
                    /* enable the all other a53 cores*/
                    socCpuCores |= (7u << CSL_CORE_ID_A53SS0_1);
                }

                /* init the cpu */
                App_PowerUpCpu(&bootCpuInfo[coreIdx]);


                /*note: I noticed that M4F works only when first power up the cpu then copy data (which is not the case for A53 and R5F)*/
                /*copy binary to the cpu memory space */
                memcpy((void*)EntryPoint , (const void*)gAppImageBuf ,gAppImageSize);

                /* force the data to be copied from the cache to the physical memory before jumping to the application */
                CacheP_wb((void *)EntryPoint, gAppImageSize, CacheP_TYPE_ALL);
            }
            else
            {
              DebugP_log("[Warning] binary file \"%s\" is not found!\r\n",pBinaryFiles[coreIdx]);
            }
            
            coreIdx++;
        }

        Bootloader_profileUpdateAppimageSize(appImageSize);
        Bootloader_profileUpdateMediaAndClk(BOOTLOADER_MEDIA_SD, 0);
        
        if(socCpuCores > 0)
        {
            /* Reset self cluster, both Core0 and Core 1. Init RAMs and run the app  */
            Bootloader_profileAddProfilePoint("SBL End");
            //Bootloader_profilePrintProfileLog();
            DebugP_log("Image loading done, switching to application ...\r\n");
            UART_flushTxFifo(gUartHandle[CONFIG_UART0]);
        }
        else
        {
           DebugP_log("nothing to load...\r\n");
           DebugP_log("Please copy a valid binary file(s) into the SD card and try to boot again!\r\n");
        }

        if(socCpuCores > 0)
        {
            /*run other cores and jump to application */
            status = App_runCpus();

            if(status != SystemP_SUCCESS)
            {
                DebugP_log("Some core(s) have failed to start!!\r\n");
            }
        }
        Bootloader_closeDma();
    }


    DebugP_log("********* SBL End *********\r\n");

    /* Call DPL deinit to close the tick timer and disable interrupts before jumping to the application */
    Dpl_deinit();

    if(socCpuCores > 0)
    {
    /* turn on leds */
        *(volatile uint32_t*)(0x000F400C) &= ~(1ul <<21);
        *(volatile uint32_t*)(0x000F4010) &= ~(1ul <<21);
        *(volatile uint32_t*)(0x000F4014) &= ~(1ul <<21);
        *(volatile uint32_t*)(0x000F4018) &= ~(1ul <<21);

        *(volatile uint32_t*)(0x00600018) |= 0x78;
        *(volatile uint32_t*)(0x00600010) &= ~(0x78);
        *(volatile uint32_t*)(0x00600014) |= 0x78;
    }

    if(0 != (socCpuCores & (1u << CSL_CORE_ID_R5FSS0_0)))
    {
         /* jump to R5F application */
        ((void(*)(void))((uintptr_t)bootCpuInfo[CSL_CORE_ID_R5FSS0_0].entryPoint))();
    }
    else
    {
        __asm("b .");
    }


    return 0;
}

//-----------------------------------------------------------------------------------------
/// \brief  
///
/// \param  
///
/// \return 
//-----------------------------------------------------------------------------------------

int App_OpenloadableImage(char* imageName)
{
    int status = SystemP_SUCCESS;
    /* File I/O */
    char fullPath[128];
    sprintf(fullPath, "/sd0/%s", imageName);

    /* Open app file */
    FF_FILE *appFp = ff_fopen(fullPath, "rb");

    /* Check if file open succeeded */
    if(appFp == NULL)
    {
        status =  SystemP_FAILURE;
    }
    else
    {
        /* Check file size */
        uint32_t fileSize = ff_filelength(appFp);

        if(fileSize >= BOOTLOADER_APPIMAGE_MAX_FILE_SIZE)
        {
            /* Application size more than buffer size, abort */
            status = SystemP_FAILURE;
            DebugP_log("Appimage size exceeded limit !!\r\n");
        }
        else
        {
            /* Read the file into RAM buffer */
            memset(gAppImageBuf, 0x0, sizeof(gAppImageBuf));
            gAppImageSize = fileSize;
            ff_fread(gAppImageBuf, fileSize, 1, appFp);
        }

        /* Close the file */
        ff_fclose(appFp);
    }

    return status;
}

//-----------------------------------------------------------------------------------------
/// \brief  
///
/// \param  
///
/// \return 
//-----------------------------------------------------------------------------------------

int32_t App_PowerUpCpu(Bootloader_CpuInfo *cpuInfo)
{
    int32_t status = SystemP_SUCCESS;

    if(CSL_CORE_ID_M4FSS0_0 == cpuInfo->cpuId)
    {
        status |= Bootloader_socCpuRequest(cpuInfo->cpuId);
        status |= Bootloader_socCpuSetClock(cpuInfo->cpuId, cpuInfo->clkHz);
        status |= Bootloader_socCpuPowerOnReset(cpuInfo->cpuId, NULL);
        if(SystemP_SUCCESS != status)
        {
            DebugP_log("failed to boot M4F core !\r\n");
        }
        Bootloader_profileAddCore(CSL_CORE_ID_M4FSS0_0);
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_M4FSS0_0)");
    }

    if(CSL_CORE_ID_R5FSS0_0 == cpuInfo->cpuId)
    {
         if(SystemP_SUCCESS != Bootloader_loadSelfCpu(NULL, cpuInfo))
        {
            DebugP_log("failed to boot R5F core !\r\n");
        }
        Bootloader_profileAddCore(CSL_CORE_ID_R5FSS0_0);
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_R5FSS0_0)");
    }

    if(CSL_CORE_ID_A53SS0_0 == cpuInfo->cpuId)
    {
        /* enable all cortex-A53 cores */
        
        for(uint32_t core = CSL_CORE_ID_A53SS0_0; core < (CSL_CORE_ID_A53SS0_0 + 4); core++)
        {
            status |= Bootloader_socCpuRequest(core);
            status |= Bootloader_socCpuSetClock(core, cpuInfo->clkHz);
            status |= Bootloader_socCpuPowerOnReset(core, NULL);
            if(SystemP_SUCCESS != status)
            {
                DebugP_log("failed to boot A53-%d core !\r\n",core);
            }
        }
        Bootloader_profileAddCore(CSL_CORE_ID_A53SS0_0);
        Bootloader_profileAddCore(CSL_CORE_ID_A53SS0_1);
        Bootloader_profileAddCore(CSL_CORE_ID_A53SS1_0);
        Bootloader_profileAddCore(CSL_CORE_ID_A53SS1_1);
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_A53SS0_0)");
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_A53SS0_1)");
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_A53SS1_0)");
        Bootloader_profileAddProfilePoint("App_loadImages(CSL_CORE_ID_A53SS1_1)");
    }
    return status;
}

//-----------------------------------------------------------------------------------------
/// \brief  
///
/// \param  
///
/// \return 
//-----------------------------------------------------------------------------------------
int32_t App_runCpus(void)
{
    int32_t status = SystemP_SUCCESS;
    uint8_t cpuId;

    for(cpuId = 0; cpuId < CSL_CORE_ID_MAX; cpuId++)
    {
        if((socCpuCores & (1u << cpuId)) != 0)
        {
            status = Bootloader_runCpu(NULL, &bootCpuInfo[cpuId]);

            if(status == SystemP_FAILURE)
            {
                Bootloader_powerOffCpu(NULL, &bootCpuInfo[cpuId]);
            }
        }
    }
    return status;
}
//-----------------------------------------------------------------------------------------
/// \brief  
///
/// \param  
///
/// \return 
//-----------------------------------------------------------------------------------------

int32_t APP_socOpenSramFirewalls(void)
{
    int32_t status = SystemP_FAILURE;

    /* Unlock HSM firewalls. */
    const struct tisci_msg_fwl_set_firewall_region_req fwl_set_req =
    {
        .fwl_id = 1u,
        .region = 0,
        .n_permission_regs = 3u,
        .control = 0x20AU,
        .permissions[0] = 0xC3FFFF,
        .permissions[1] = 0xC3FFFF,
        .permissions[2] = 0xC3FFFF,
        .start_address  = 0x80000000u,
        .end_address    = 0x9FFFFFFFu, //512 MB
    };
    struct tisci_msg_fwl_set_firewall_region_resp fwl_set_resp = { 0 };

    status = Sciclient_firewallSetRegion(&fwl_set_req, &fwl_set_resp, SystemP_TIMEOUT);

    if(status == SystemP_SUCCESS)
    {
        const struct tisci_msg_fwl_set_firewall_region_req fwl_set_req1 =
        {
            .fwl_id = 64U,
            .region = 0,
            .n_permission_regs = 3u,
            .control = 0x20AU,
            .permissions[0] = 0xC3FFFF,
            .permissions[1] = 0xC3FFFF,
            .permissions[2] = 0xC3FFFF,
            .start_address  = 0x70000000,
            .end_address    = 0x7000FFFF,
        };
        struct tisci_msg_fwl_set_firewall_region_resp fwl_set_resp1 = { 0 };

        status = Sciclient_firewallSetRegion(&fwl_set_req1, &fwl_set_resp1, SystemP_TIMEOUT);
    }


    return status;
}

