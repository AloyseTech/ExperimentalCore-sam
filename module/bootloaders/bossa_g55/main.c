/*
  Copyright (c) 2015 Arduino LLC.  All right reserved.
  Copyright (c) 2015 Atmel Corporation/Thibaut VIARD.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <sam.h>
#include "sam_ba_monitor.h"
#include "sam_ba_serial.h"
#include "variant_definitions.h"
#include "variant_driver_led.h"
#include "sam_ba_usb.h"
#include "sam_ba_cdc.h"

extern uint32_t __sketch_vectors_ptr; // Exported value from linker script
extern void board_init(void);

#if (defined DEBUG) && (DEBUG == 1)
volatile uint32_t* pulSketch_Start_Address;
#endif

static volatile uint32_t ul_usb_cdc_enabled = 0;

/**
 * \brief Check the application startup condition
 *
 */
static void check_start_application(void)
{
//  LED_init();
//  LED_off();

#if (!defined DEBUG) || ((defined DEBUG) && (DEBUG == 0))
uint32_t* pulSketch_Start_Address;
#endif

  /*
   * Test sketch stack pointer @ &__sketch_vectors_ptr
   * Stay in SAM-BA if value @ (&__sketch_vectors_ptr) == 0xFFFFFFFF (Erased flash cell value)
   */
  if (__sketch_vectors_ptr == 0xFFFFFFFF)
  {
    /* Stay in bootloader */
    return;
  }

  /*
   * Load the sketch Reset Handler address
   * __sketch_vectors_ptr is exported from linker script and point on first 32b word of sketch vector table
   * First 32b word is sketch stack
   * Second 32b word is sketch entry point: Reset_Handler()
   */
  pulSketch_Start_Address = &__sketch_vectors_ptr ;
  pulSketch_Start_Address++ ;

  /*
   * Test vector table address of sketch @ &__sketch_vectors_ptr
   * Stay in SAM-BA if this function is not aligned enough, ie not valid
   */
  if ( ((uint32_t)(&__sketch_vectors_ptr) & ~SCB_VTOR_TBLOFF_Msk) != 0x00)
  {
    /* Stay in bootloader */
    return;
  }

#if defined(BOOT_LOAD_PIN)
  volatile bool boot_en;

  // Read the BOOT_LOAD_PIN status
  boot_en = (BOOT_LOAD_PIN_PORT->PIO_PDSR & BOOT_LOAD_PIN_MASK) == 0;

  // Check the bootloader enable condition
  if (!boot_en)
  {
    // Stay in bootloader
    return;
  }
#endif

//  LED_on();

  /* Rebase the Stack Pointer */
  __set_MSP( (uint32_t)(__sketch_vectors_ptr) );

  /* Rebase the vector table base address */
  SCB->VTOR = ((uint32_t)(&__sketch_vectors_ptr) & SCB_VTOR_TBLOFF_Msk);

  /* Jump to application Reset Handler in the application */
  asm("bx %0"::"r"(*pulSketch_Start_Address));
}

/**
 *  \brief SAMD21 SAM-BA Main loop.
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{
#ifdef BOARD_HAS_USB_CDC
  P_USB_CDC pCdc;
#endif // BOARD_HAS_USB_CDC

  /* Jump in application if condition is satisfied */
  check_start_application();

  /* We have determined we should stay in the monitor. */
  /* System initialization */
  variant_init();
  __enable_irq();

#ifdef BOARD_HAS_USART
  /* UART is enabled in all cases */
  samba_serial_open();
#endif // BOARD_HAS_USART

#ifdef BOARD_HAS_USB_CDC
  pCdc = usb_init();
#endif // BOARD_HAS_USB_CDC

  /* Wait for a complete enum on usb or a '#' char on serial line */
  while (1)
  {
#ifdef BOARD_HAS_USB_CDC
    if (pCdc->IsConfigured(pCdc) != 0)
    {
      ul_usb_cdc_enabled = 1;
    }

    /* Check if a USB enumeration has succeeded and if comm port has been opened */
    if (ul_usb_cdc_enabled == 1)
    {
      sam_ba_monitor_init(SAM_BA_INTERFACE_USBCDC);
      /* SAM-BA on USB loop */
      while( 1 )
      {
        sam_ba_monitor_run();
      }
    }
#endif // BOARD_HAS_USB_CDC

#ifdef BOARD_HAS_USART
    /* Check if a '#' has been received */
    if ((ul_usb_cdc_enabled == 0) && samba_serial_sharp_received())
    {
      sam_ba_monitor_init(SAM_BA_INTERFACE_USART);
      /* SAM-BA on Serial loop */
      while(1)
      {
        sam_ba_monitor_run();
      }
    }
#endif // BOARD_HAS_USART
  }
}
