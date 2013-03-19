
#include "helper.h"
#include "flash.h"
#include "ihex.h"
#include <ch.h>
#include <hal.h>
#include <ff.h>

int flashIHexFile(FIL* file)
{
    IHexRecord irec;
    flashsector_t sector;
    bool_t erasedSectors[FLASH_SECTOR_COUNT] = { FALSE };
    flashaddr_t baseAddress = 0;
    flashaddr_t address = 0;

    while (Read_IHexRecord(&irec, file) == IHEX_OK)
    {
        switch (irec.type)
        {
        case IHEX_TYPE_00:    /**< Data Record */
            /* Compute the target address in flash */
            address = baseAddress + irec.address;

            /* Erase the corresponding addresses if needed */
            for (sector = flashSectorAt(address); sector <= flashSectorAt(address + irec.dataLen - 1); ++sector)
            {
                /* Check if the sector has been erased during this IHex flashing procedure to
                   prevent erasing already written data */
                if (erasedSectors[sector] == TRUE)
                    continue;

                /* Check if the sector in flash needs to be erased */
                if (flashIsErased(flashSectorBegin(sector), flashSectorSize(sector)) == FALSE)
                {
                    /* Erase the sector */
                    if (flashSectorErase(sector) != FLASH_RETURN_SUCCESS)
                        return BOOTLOADER_ERROR_BADFLASH;
                }

                /* Set the erased flag to prevent erasing the same sector twice during
                   the IHex flashing procedure */
                erasedSectors[sector] = TRUE;
            }

            /* Write the data in flash */
            if (flashWrite(address, (const char*)irec.data, irec.dataLen) != FLASH_RETURN_SUCCESS)
                return BOOTLOADER_ERROR_BADFLASH;
            break;

        case IHEX_TYPE_04:    /**< Extended Linear Address Record */
            /* Compute the base address of the following data records */
            baseAddress = irec.data[0];
            baseAddress <<= 8;
            baseAddress += irec.data[1];
            baseAddress <<= 16;
            break;

        case IHEX_TYPE_01:    /**< End of File Record */
            /* Check that the end of file record is at the end of the file... */
            return f_eof(file) ? BOOTLOADER_SUCCESS : BOOTLOADER_ERROR_BADHEX;

        case IHEX_TYPE_05:    /**< Start Linear Address Record */
            /* Ignored */
            break;

        case IHEX_TYPE_02:    /**< Extended Segment Address Record */
        case IHEX_TYPE_03:    /**< Start Segment Address Record */
            /* Not supported */
            return BOOTLOADER_ERROR_BADHEX;
        }
    }

    return BOOTLOADER_ERROR_BADHEX;
}

void flashJumpApplication(uint32_t address)
{
    typedef void (*funcPtr)(void);

    u32 jumpAddr = *(vu32*)(address + 0x04); /* reset ptr in vector table */
    funcPtr usrMain = (funcPtr)jumpAddr;

    /* Reset all interrupts to default */
    chSysDisable();

    /* Clear pending interrupts just to be on the save side */
    SCB_ICSR = ICSR_PENDSVCLR;

    /* Disable all interrupts */
    int i;
    for(i = 0; i < 8; ++i)
        NVIC->ICER[i] = NVIC->IABR[i];

    /* Set stack pointer as in application's vector table */
    __set_MSP(*(vu32*)address);
    usrMain();
}
