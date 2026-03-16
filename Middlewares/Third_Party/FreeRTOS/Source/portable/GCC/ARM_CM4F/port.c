/*
 * FreeRTOS Kernel V10.3.1
 * GCC/ARM_CM4F port
 */

#include "FreeRTOS.h"
#include "task.h"

#ifndef __VFP_FP__
	#error Compile with -mfpu=fpv4-sp-d16 -mfloat-abi=hard
#endif

#if configMAX_SYSCALL_INTERRUPT_PRIORITY == 0
	#error configMAX_SYSCALL_INTERRUPT_PRIORITY must not be 0
#endif

/* NVIC/SysTick registers */
#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )
#define portNVIC_SYSPRI2_REG				( * ( ( volatile uint32_t * ) 0xe000ed20 ) )
#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_PENDSV_PRI					( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 16UL )
#define portNVIC_SYSTICK_PRI				( ( ( uint32_t ) configKERNEL_INTERRUPT_PRIORITY ) << 24UL )

#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ		configCPU_CLOCK_HZ
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )
#else
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )
#endif

/* Initial stack frame */
#define portINITIAL_XPSR		( 0x01000000UL )
#define portINITIAL_EXC_RETURN	( 0xFFFFFFFDUL )
#define portSTART_ADDRESS_MASK	( ( StackType_t ) 0xFFFFFFFEUL )

/* FPU lazy stacking */
#define portFPCCR				( ( volatile uint32_t * ) 0xe000ef34 )
#define portASPEN_AND_LSPEN		( 0x3UL << 30UL )

/* Critical nesting counter */
static UBaseType_t uxCriticalNesting = 0xaaaaaaaa;

/* Handler declarations */
void xPortPendSVHandler( void ) __attribute__ (( naked ));
void xPortSysTickHandler( void );
void vPortSVCHandler( void ) __attribute__ (( naked ));
void vPortSetupTimerInterrupt( void );
static void prvStartFirstTask( void ) __attribute__ (( naked ));
static void prvTaskExitError( void );

/*-----------------------------------------------------------*/

static void prvTaskExitError( void )
{
	volatile uint32_t ulDummy = 0;
	configASSERT( ulDummy == pdFALSE );
	portDISABLE_INTERRUPTS();
	for( ;; );
}

/*-----------------------------------------------------------*/

/* SVC handler — starts the first task */
void vPortSVCHandler( void )
{
	__asm volatile (
		"	ldr	r3, pxCurrentTCBConst2		\n"
		"	ldr r1, [r3]					\n"
		"	ldr r0, [r1]					\n"
		"	ldm r0!, {r4-r11, r14}			\n"
		"	msr psp, r0						\n"
		"	isb								\n"
		"	mov r0, #0						\n"
		"	msr	basepri, r0					\n"
		"	bx r14							\n"
		"	.align 4						\n"
		"pxCurrentTCBConst2: .word pxCurrentTCB	\n"
	);
}

/*-----------------------------------------------------------*/

static void prvStartFirstTask( void )
{
	__asm volatile (
		" ldr r0, =0xE000ED08	\n" /* VTOR */
		" ldr r0, [r0]			\n"
		" ldr r0, [r0]			\n" /* initial MSP value */
		" msr msp, r0			\n"
		" cpsie i				\n"
		" cpsie f				\n"
		" dsb					\n"
		" isb					\n"
		" svc 0					\n"
		" nop					\n"
	);
}

/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler( void )
{
	configASSERT( configMAX_SYSCALL_INTERRUPT_PRIORITY );

	/* Set PendSV and SysTick to lowest priority */
	portNVIC_SYSPRI2_REG |= portNVIC_PENDSV_PRI;
	portNVIC_SYSPRI2_REG |= portNVIC_SYSTICK_PRI;

	/* Configure tick timer */
	vPortSetupTimerInterrupt();

	uxCriticalNesting = 0;

	/* Enable FPU lazy stacking */
	*portFPCCR |= portASPEN_AND_LSPEN;

	prvStartFirstTask();
	prvTaskExitError();
	return 0;
}

void vPortEndScheduler( void )
{
	configASSERT( ( volatile void * ) NULL );
}

/*-----------------------------------------------------------*/

void vPortEnterCritical( void )
{
	portDISABLE_INTERRUPTS();
	uxCriticalNesting++;
	__asm volatile( "dsb" ::: "memory" );
	__asm volatile( "isb" );
}

void vPortExitCritical( void )
{
	configASSERT( uxCriticalNesting );
	uxCriticalNesting--;
	if( uxCriticalNesting == 0 )
	{
		portENABLE_INTERRUPTS();
	}
}

/*-----------------------------------------------------------*/

/* PendSV — context switch */
void xPortPendSVHandler( void )
{
	__asm volatile
	(
		"	mrs r0, psp							\n"
		"	isb									\n"
		"										\n"
		"	ldr	r3, pxCurrentTCBConst			\n"
		"	ldr	r2, [r3]						\n"
		"										\n"
		"	tst r14, #0x10						\n"
		"	it eq								\n"
		"	vstmdbeq r0!, {s16-s31}				\n"
		"										\n"
		"	stmdb r0!, {r4-r11, r14}			\n"
		"	str r0, [r2]						\n"
		"										\n"
		"	stmdb sp!, {r0, r3}					\n"
		"	mov r0, %0							\n"
		"	msr basepri, r0						\n"
		"	dsb									\n"
		"	isb									\n"
		"	bl vTaskSwitchContext				\n"
		"	mov r0, #0							\n"
		"	msr basepri, r0						\n"
		"	ldmia sp!, {r0, r3}					\n"
		"										\n"
		"	ldr r1, [r3]						\n"
		"	ldr r0, [r1]						\n"
		"										\n"
		"	ldmia r0!, {r4-r11, r14}			\n"
		"										\n"
		"	tst r14, #0x10						\n"
		"	it eq								\n"
		"	vldmiaeq r0!, {s16-s31}				\n"
		"										\n"
		"	msr psp, r0							\n"
		"	isb									\n"
		"	bx r14								\n"
		"										\n"
		"	.align 4							\n"
		"pxCurrentTCBConst: .word pxCurrentTCB	\n"
		::"i"(configMAX_SYSCALL_INTERRUPT_PRIORITY)
	);
}

/*-----------------------------------------------------------*/

void xPortSysTickHandler( void )
{
	portDISABLE_INTERRUPTS();
	if( xTaskIncrementTick() != pdFALSE )
	{
		portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
	}
	portENABLE_INTERRUPTS();
}

/*-----------------------------------------------------------*/

__attribute__(( weak )) void vPortSetupTimerInterrupt( void )
{
	portNVIC_SYSTICK_CTRL_REG = 0UL;
	portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
	portNVIC_SYSTICK_LOAD_REG = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
	portNVIC_SYSTICK_CTRL_REG = portNVIC_SYSTICK_CLK_BIT | portNVIC_SYSTICK_INT_BIT | portNVIC_SYSTICK_ENABLE_BIT;
}

/*-----------------------------------------------------------*/

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_XPSR;                               /* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) pxCode & portSTART_ADDRESS_MASK; /* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) prvTaskExitError;               /* LR */
	pxTopOfStack -= 5;                                              /* R12, R3, R2, R1 */
	*pxTopOfStack = ( StackType_t ) pvParameters;                   /* R0 */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_EXC_RETURN;                        /* EXC_RETURN (use PSP, thumb) */
	pxTopOfStack -= 8;                                              /* R11..R4 */

	return pxTopOfStack;
}

/*-----------------------------------------------------------*/

#ifdef configASSERT
void vPortValidateInterruptPriority( void )
{
	/* Simplified: just check we are in an interrupt. */
	uint32_t ulCurrentInterrupt;
	__asm volatile( "mrs %0, ipsr" : "=r"( ulCurrentInterrupt ) :: "memory" );
	configASSERT( ulCurrentInterrupt != 0 );
}
#endif
