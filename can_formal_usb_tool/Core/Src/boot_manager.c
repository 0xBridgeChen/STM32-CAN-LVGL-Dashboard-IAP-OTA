#include "boot_manager.h"
#include "stm32f1xx_hal.h"
#include "stdio.h"

#define AC5_MODE			1

	
typedef void (*APP_FUNC)(void); 				//函数指针类型定义

#if (AC5_MODE == 1)			//AC5
__asm void MSR_MSP(uint32_t addr)
{
    MSR MSP, r0
    BX r14;
}
#else			//AC6
void MSR_MSP(uint32_t addr) 
{
    __ASM("MSR MSP, r0"); 
    __ASM("BX r14");
}
#endif


#define CLOSE_ALL_INT()  __set_PRIMASK(1)	//关闭所有中断


uint8_t boot_check_stack2jump_app(uint32_t addr) // APP_ADDR
{
#if (AC5_MODE == 1)			//TODO:https://www.armbbs.cn/forum.php?mod=viewthread&tid=110358
		APP_FUNC jump2app; // 定义一个函数指针	
#else
		static APP_FUNC jump2app; // 定义一个函数指针
#endif	

    /* 栈顶地址是否合法 */		
    if (((*(__IO uint32_t *)addr) & 0x2FFE0000) == 0x20000000)
    {
        
        /* 设置栈指针 */
        MSR_MSP(addr);
        /* 获取复位地址 */
        jump2app = (APP_FUNC) * (volatile uint32_t *)(addr + 4);
        /* 设置栈指针 */
        __set_MSP(*(volatile uint32_t *)addr);

        /* 跳转之前关闭相应的中断 */
        CLOSE_ALL_INT();
        /* 跳转至APP */
        jump2app();

        printf("Bootloader end load app\r\n");
    }
    else
    {
        printf("APP Not Found!\r\n");
    }

    return 0;
}

#if 0

#endif
