/*
 * FreeRTOS Kernel V10.0.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/* Standard includes. */
#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

#ifndef configSETUP_TICK_INTERRUPT
	#error configSETUP_TICK_INTERRUPT() must be defined.  See http://www.freertos.org/Using-FreeRTOS-on-Cortex-A-Embedded-Processors.html
#endif /* configSETUP_TICK_INTERRUPT */

/* Some vendor specific files default configCLEAR_TICK_INTERRUPT() in
portmacro.h. */
#ifndef configCLEAR_TICK_INTERRUPT
	#define configCLEAR_TICK_INTERRUPT()
#endif

/* A critical section is exited when the critical section nesting count reaches
this value. */
#define portNO_CRITICAL_NESTING			( ( size_t ) 0 )

/* Tasks are not created with a floating point context, but can be given a
floating point context after they have been created.  A variable is stored as
part of the tasks context that holds portNO_FLOATING_POINT_CONTEXT if the task
does not have an FPU context, or any other value if the task does have an FPU
context. */
#define portNO_FLOATING_POINT_CONTEXT	( ( StackType_t ) 0 )

/* Constants required to setup the initial task context. */
#define portSP_ELx						( ( StackType_t ) 0x01 )
#define portSP_EL0						( ( StackType_t ) 0x00 )

#define portEL1							( ( StackType_t ) 0x04 )
#define portINITIAL_PSTATE				( portEL1 | portSP_EL0 )

/* Masks all bits in the APSR other than the mode bits. */
#define portAPSR_MODE_BITS_MASK			( 0x0C )

/* Used in the ASM code. */
__attribute__(( used )) const uint64_t ulCORE0_INT_SRC = 0x40000060;

/*-----------------------------------------------------------*/

/*
 * Starts the first task executing.  This function is necessarily written in
 * assembly code so is implemented in portASM.s.
 */
extern void vPortRestoreTaskContext( void );

/*-----------------------------------------------------------*/

/* A variable is used to keep track of the critical section nesting.  This
variable has to be stored as part of the task context and must be initialised to
a non zero value to ensure interrupts don't inadvertently become unmasked before
the scheduler starts.  As it is stored as part of the task context it will
automatically be set to 0 when the first task is started. */
volatile uint64_t ullCriticalNesting = 9999ULL;

/* Saved as part of the task context.  If ullPortTaskHasFPUContext is non-zero
then floating point context must be saved and restored for the task. */
uint64_t ullPortTaskHasFPUContext = pdFALSE;

/* Set to 1 to pend a context switch from an ISR. */
uint64_t ullPortYieldRequired = pdFALSE;

/* Counts the interrupt nesting depth.  A context switch is only performed if
if the nesting depth is 0. */
uint64_t ullPortInterruptNesting = 0;

/*-----------------------------------------------------------*/
/*
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	/* Setup the initial stack of the task.  The stack is set exactly as
	expected by the portRESTORE_CONTEXT() macro. */

	/* First all the general purpose registers. */
	pxTopOfStack--;
	*pxTopOfStack = 0x0101010101010101ULL;	/* R1 */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pvParameters; /* R0 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0303030303030303ULL;	/* R3 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0202020202020202ULL;	/* R2 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0505050505050505ULL;	/* R5 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0404040404040404ULL;	/* R4 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0707070707070707ULL;	/* R7 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0606060606060606ULL;	/* R6 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0909090909090909ULL;	/* R9 */
	pxTopOfStack--;
	*pxTopOfStack = 0x0808080808080808ULL;	/* R8 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1111111111111111ULL;	/* R11 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1010101010101010ULL;	/* R10 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1313131313131313ULL;	/* R13 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1212121212121212ULL;	/* R12 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1515151515151515ULL;	/* R15 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1414141414141414ULL;	/* R14 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1717171717171717ULL;	/* R17 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1616161616161616ULL;	/* R16 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1919191919191919ULL;	/* R19 */
	pxTopOfStack--;
	*pxTopOfStack = 0x1818181818181818ULL;	/* R18 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2121212121212121ULL;	/* R21 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2020202020202020ULL;	/* R20 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2323232323232323ULL;	/* R23 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2222222222222222ULL;	/* R22 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2525252525252525ULL;	/* R25 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2424242424242424ULL;	/* R24 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2727272727272727ULL;	/* R27 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2626262626262626ULL;	/* R26 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2929292929292929ULL;	/* R29 */
	pxTopOfStack--;
	*pxTopOfStack = 0x2828282828282828ULL;	/* R28 */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x00;	/* XZR - has no effect, used so there are an even number of registers. */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) 0x00;	/* R30 - procedure call link register. */
	pxTopOfStack--;

	*pxTopOfStack = portINITIAL_PSTATE;
	pxTopOfStack--;

	*pxTopOfStack = ( StackType_t ) pxCode; /* Exception return address. */
	pxTopOfStack--;

	/* The task will start with a critical nesting count of 0 as interrupts are
	enabled. */
	*pxTopOfStack = portNO_CRITICAL_NESTING;
	pxTopOfStack--;

	/* The task will start without a floating point context.  A task that uses
	the floating point hardware must call vPortTaskUsesFPU() before executing
	any floating point instructions. */
	*pxTopOfStack = portNO_FLOATING_POINT_CONTEXT;

	return pxTopOfStack;
}
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
	uint32_t ulAPSR;

	/* At the time of writing, the BSP only supports EL3. */
	__asm volatile ( "MRS %0, CurrentEL" : "=r" ( ulAPSR ) );
	ulAPSR &= portAPSR_MODE_BITS_MASK;

	configASSERT( ulAPSR == portEL1 );
	if( ulAPSR == portEL1 )
	{
		{
			/* Interrupts are turned off in the CPU itself to ensure a tick does
			not execute	while the scheduler is being started.  Interrupts are
			automatically turned back on in the CPU when the first task starts
			executing. */
			portDISABLE_INTERRUPTS();

			/* Start the timer that generates the tick ISR. */
			configSETUP_TICK_INTERRUPT();

			/* Start the first task executing. */
			vPortRestoreTaskContext();
		}
	}

	return 0;
}
/*-----------------------------------------------------------*/

void vPortEndScheduler( void )
{
	/* Not implemented in ports where there is nothing to return to.
	Artificially force an assert. */
	configASSERT( ullCriticalNesting == 1000ULL );
}
/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();

	/* Now interrupts are disabled ullCriticalNesting can be accessed
	directly.  Increment ullCriticalNesting to keep a count of how many times
	portENTER_CRITICAL() has been called. */
	ullCriticalNesting++;

	/* This is not the interrupt safe version of the enter critical function so
	assert() if it is being called from an interrupt context.  Only API
	functions that end in "FromISR" can be used in an interrupt.  Only assert if
	the critical nesting count is 1 to protect against recursive calls if the
	assert function also uses a critical section. */
	if( ullCriticalNesting == 1ULL )
	{
		configASSERT( ullPortInterruptNesting == 0 );
	}
}
/*-----------------------------------------------------------*/

void vPortExitCritical( void )
{
	if( ullCriticalNesting > portNO_CRITICAL_NESTING )
	{
		/* Decrement the nesting count as the critical section is being
		exited. */
		ullCriticalNesting--;

		/* If the nesting level has reached zero then all interrupt
		priorities must be re-enabled. */
		if( ullCriticalNesting == portNO_CRITICAL_NESTING )
		{
			/* Critical nesting has reached zero so interrupts
			should be enabled. */
			portENABLE_INTERRUPTS();
		}
	}
}
/*-----------------------------------------------------------*/

void FreeRTOS_Tick_Handler( void )
{
	/* Interrupts should not be enabled before this point. */
	#if( configASSERT_DEFINED == 1 )
	{
		uint32_t ulMaskBits;

		__asm volatile( "mrs %0, daif" : "=r"( ulMaskBits ) :: "memory" );
		configASSERT( ( ulMaskBits & portDAIF_I ) != 0 );
	}
	#endif /* configASSERT_DEFINED */

	/* Ok to enable interrupts after the interrupt source has been cleared. */
	configCLEAR_TICK_INTERRUPT();
	portENABLE_INTERRUPTS();

	/* Increment the RTOS tick. */
	if( xTaskIncrementTick() != pdFALSE )
	{
		ullPortYieldRequired = pdTRUE;
	}
}
/*-----------------------------------------------------------*/

void *memcpy(void *dst, const void *src, size_t n)
{
	/* copy per 1 byte */
	const char *p = src;
	char *q = dst;

	while (n--) {
		*q++ = *p++;
	}

	return dst;
}

void *memset(void *s, int c, size_t n)
{
	/* set per 1 byte */
	char *q = s;

	while (n--) {
		*q++ = c;
	}

	return s;
}

#ifdef ATOMIC_16_TESTS

#define BF_ATOMIC_GEN		0
#define BF_ATOMIC_GEN_N		1
#define BF_ATOMIC_IMPL		2

#define BF_ATOMIC_TYPE		BF_ATOMIC_IMPL

/*
	Built-in Function: type __atomic_load_n (type *ptr, int memorder)

		This built-in function implements an atomic load operation. It returns the contents of *ptr.

		The valid memory order variants are __ATOMIC_RELAXED, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE, and __ATOMIC_CONSUME.
*/
inline __uint128_t __atomic_load_16(const volatile void *ptr, int memorder)
#if (BF_ATOMIC_TYPE == BF_ATOMIC_GEN)
{
	__uint128_t ret;

	__atomic_load(&ret, (__uint128_t *)ptr, memorder);

	return ret;
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_GEN_N)
{
	return __atomic_load_n((__uint128_t *)ptr, memorder);
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_IMPL)
{
	__uint128_t ret;

	(void)memorder;	// Avoid warning about unused parameter

	portENTER_CRITICAL();

	ret = *(__uint128_t *)ptr;

	portEXIT_CRITICAL();

	return ret;
}
#endif

/*
	Built-in Function: void __atomic_store_n (type *ptr, type val, int memorder)

		This built-in function implements an atomic store operation. It writes val into *ptr.

		The valid memory order variants are __ATOMIC_RELAXED, __ATOMIC_SEQ_CST, and __ATOMIC_RELEASE.
*/
inline void __atomic_store_16(volatile void *ptr, __uint128_t val, int memorder)
#if (BF_ATOMIC_TYPE == BF_ATOMIC_GEN)
{
	__atomic_store((__uint128_t *)ptr, &val, memorder);
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_GEN_N)
{
	__atomic_store_n((__uint128_t *)ptr, val, memorder);
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_IMPL)
{
	(void)memorder;	// Avoid warning about unused parameter

	portENTER_CRITICAL();

	*(__uint128_t *)ptr = val;

	portEXIT_CRITICAL();
}
#endif

/*
	Built-in Function: bool __atomic_compare_exchange_n (type *ptr, type *expected, type desired, bool weak, int success_memorder, int failure_memorder)

		This built-in function implements an atomic compare and exchange operation. This compares the contents of *ptr with the
			contents of *expected. If equal, the operation is a read-modify-write operation that writes desired into *ptr. If they are
			not equal, the operation is a read and the current contents of *ptr are written into *expected. weak is true for weak
			compare_exchange, which may fail spuriously, and false for the strong variation, which never fails spuriously. Many targets only
			offer the strong variation and ignore the parameter. When in doubt, use the strong variation.

		If desired is written into *ptr then true is returned and memory is affected according to the memory order specified by success_memorder. There 
			are no restrictions on what memory order can be used here.

		Otherwise, false is returned and memory is affected according to failure_memorder. This memory order cannot be __ATOMIC_RELEASE nor 
			__ATOMIC_ACQ_REL. It also cannot be a stronger order than that specified by success_memorder.
*/
inline _Bool __atomic_compare_exchange_16(volatile void *ptr, void *expected, __uint128_t desired, _Bool weak,  int success_memorder,  int failure_memorder)
#if (BF_ATOMIC_TYPE == BF_ATOMIC_GEN)
{
	return __atomic_compare_exchange((__uint128_t *)ptr, (__uint128_t *)expected, &desired, weak, success_memorder, failure_memorder);
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_GEN_N)
{
	return __atomic_compare_exchange_n((__uint128_t *)ptr, (__uint128_t *)expected, desired, weak, success_memorder, failure_memorder);
}
#elif (BF_ATOMIC_TYPE == BF_ATOMIC_IMPL)
{
	(void)weak;	// Avoid warning about unused parameter
	(void)success_memorder;	// Avoid warning about unused parameter
	(void)failure_memorder;	// Avoid warning about unused parameter

	_Bool ret;

	portENTER_CRITICAL();

	if (*(__uint128_t *)ptr == *(__uint128_t *)expected) {
		*(__uint128_t *)ptr = desired;
		ret = 1;
	}
	else {
		*(__uint128_t *)expected = *(__uint128_t *)ptr;
		ret = 0;
	}

	portEXIT_CRITICAL();

	return ret;
}
#endif

#endif
