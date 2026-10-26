# STM32F407VET6 + ST7796：从标准库软件模拟 SPI 到 CubeMX HAL 硬件 SPI 的完整迁移记录

## 1. 文档目的与最终结论

本文完整记录了把中景园 ST7796 官方例程移植到当前 STM32F407VET6 工程的过程，包括：

- 原代码采用了什么结构；
- 为什么不能直接复制到 CubeMX + HAL 工程；
- CubeMX 中需要配置哪些外设和引脚；
- 哪些文件可以保留，哪些代码必须删除或替换；
- 软件模拟 SPI 如何改成硬件 SPI1；
- 迁移过程中实际遇到的编译问题及其原因；
- 最终如何在 `main.c` 中初始化并显示 `CAN TEST`；
- 为什么后续优化必须以这份实机可用版本为基线。

本次迁移的参考源工程为：

```text
C:\Users\aa842\Desktop\汽车方向盘\中景园st7796技术资料\
02-3.5LCD程序源码\02-3.5LCD程序源码\
05-3.5LCD显示屏STM32F407ZG_SPI例程
```

最终实机验证工程为：

```text
C:\Users\aa842\Desktop\汽车方向盘\can_test01_ok_lcd
```

最终结果已经得到硬件验证：

```text
STM32F407VET6 能通过 SPI1 初始化 ST7796；
屏幕能够清屏并显示固定字符串 CAN TEST；
说明 CubeMX GPIO、SPI1、LCD 控制线和显示驱动调用链已经打通。
```

需要特别说明：本文记录的是 `can_test01_ok_lcd` 当时的实际引脚。后续复制出来的 APP、IAP 或触控工程可能重新分配了 `CS/DC/RES/BLK`，迁移到其他工程时必须重新核对该工程的 `.ioc`、`main.h` 和 `gpio.c`，不能只凭本文引脚机械复制。

---

## 2. 迁移前后架构发生了什么变化

### 2.1 官方例程的结构

官方例程虽然文件夹名称带有“SPI例程”，但 LCD 数据发送部分实际采用的是 GPIO 软件模拟 SPI：

```text
CPU逐条执行GPIO置位/复位语句
        ↓
手动改变SCLK
        ↓
根据当前数据位改变MOSI
        ↓
循环8次发送一个字节
```

其核心代码类似：

```c
for (i = 0; i < 8; i++)
{
    LCD_SCLK_Clr();
    if (dat & 0x80)
    {
        LCD_MOSI_Set();
    }
    else
    {
        LCD_MOSI_Clr();
    }
    LCD_SCLK_Set();
    dat <<= 1;
}
```

官方例程同时使用 STM32 标准外设库完成 GPIO 初始化，例如：

```c
RCC_AHB1PeriphClockCmd(...);
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_Init(...);
GPIO_SetBits(...);
GPIO_ResetBits(...);
```

### 2.2 迁移后的结构

当前工程由 CubeMX 生成底层初始化，数据发送交给 STM32F407 内部的 SPI1 外设：

```text
CPU把一个字节交给SPI1
        ↓
SPI1硬件自动产生SCK
        ↓
SPI1硬件自动从MOSI发送8位数据
        ↓
ST7796根据DC状态把字节解释成命令或数据
```

核心发送函数变成：

```c
HAL_SPI_Transmit(&hspi1, &dat, 1, HAL_MAX_DELAY);
```

迁移的本质不是重写整个屏幕库，而是替换最底层的“字节如何发送”和“GPIO由谁初始化”：

```text
保留：
ST7796寄存器初始化序列
LCD_WR_REG/LCD_WR_DATA
LCD_Address_Set
清屏、画点、画线、字符、数字、图片等绘图函数
字体点阵

替换：
标准库GPIO初始化
软件模拟SCLK/MOSI
标准库GPIO_SetBits/GPIO_ResetBits
旧delay_ms
```

这与学习 CAN 时的分层思路相同：上层仍然表达“发送什么”，底层外设负责“怎样形成物理波形”。

---

## 3. 原官方例程使用的引脚

原 STM32F407ZG 例程定义如下：

| 信号 | 原例程引脚 | 原用途 |
|---|---:|---|
| SCL/SCLK | PG12 | 软件模拟时钟 |
| SDA/MOSI | PD5 | 软件模拟数据 |
| RES | PD4 | LCD 硬件复位 |
| DC | PD15 | 命令/数据选择 |
| CS | PD1 | LCD 片选 |
| BLK | PE8 | 背光控制 |

官方 `lcd_init.h` 中直接把端口写死：

```c
#define LCD_SCLK_Clr() GPIO_ResetBits(GPIOG, GPIO_Pin_12)
#define LCD_MOSI_Clr() GPIO_ResetBits(GPIOD, GPIO_Pin_5)
#define LCD_RES_Clr()  GPIO_ResetBits(GPIOD, GPIO_Pin_4)
#define LCD_DC_Clr()   GPIO_ResetBits(GPIOD, GPIO_Pin_15)
#define LCD_CS_Clr()   GPIO_ResetBits(GPIOD, GPIO_Pin_1)
#define LCD_BLK_Clr()  GPIO_ResetBits(GPIOE, GPIO_Pin_8)
```

这套定义只适用于原例程自己的底层代码。当前工程要使用硬件 SPI1，就不能继续用 PG12 和 PD5 模拟时序。

---

## 4. 最终验证工程的 CubeMX 配置

以下配置来自 `can_test01_ok_lcd/can_test01.ioc` 和 CubeMX 生成代码。

### 4.1 系统时钟

```text
HSE：8 MHz
SYSCLK：168 MHz
APB2：84 MHz
```

SPI1 挂在 APB2 上。

### 4.2 SPI1 数据线

| SPI 信号 | STM32F407VET6 引脚 | CubeMX 模式 |
|---|---:|---|
| SPI1_SCK | PA5 | GPIO_AF5_SPI1 |
| SPI1_MOSI | PA7 | GPIO_AF5_SPI1 |

当时只需要向 LCD 写数据，因此没有使用 MISO。PA5、PA7 由 `HAL_SPI_MspInit()` 配置成复用功能，不再作为普通 GPIO 由 LCD 驱动手动翻转。

### 4.3 SPI1 参数

最终生成的 `MX_SPI1_Init()` 为：

```c
hspi1.Instance = SPI1;
hspi1.Init.Mode = SPI_MODE_MASTER;
hspi1.Init.Direction = SPI_DIRECTION_2LINES;
hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
hspi1.Init.NSS = SPI_NSS_SOFT;
hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
```

对应含义：

| 参数 | 含义 | 为什么这样设置 |
|---|---|---|
| Master | STM32 主机 | 屏幕是从设备，由 STM32 提供时钟 |
| 8 bit | 每帧 8 位 | ST7796 的命令和参数按字节发送 |
| MSB First | 高位先发 | 与原软件 SPI 的 `dat & 0x80` 顺序一致 |
| CPOL High | 空闲时 SCK 为高 | 对应实机可用配置 |
| CPHA 2Edge | 第二边沿采样 | 与屏幕例程匹配，实际已验证 |
| Software NSS | 软件控制片选 | LCD_CS 由普通 GPIO 管理，而不是 SPI NSS 引脚 |
| Prescaler 16 | SPI 时钟约 5.25 Mbit/s | 先使用保守且稳定的速度完成基础验证 |

这里最重要的学习点是：SPI 设备双方必须对时钟极性、时钟相位和位顺序达成一致。只看到 SCK 有波形，并不能证明屏幕按正确边沿读取了数据。

### 4.4 LCD 普通控制线

最终验证工程使用：

| LCD 信号 | STM32F407VET6 引脚 | GPIO 状态 |
|---|---:|---|
| RES | PD4 | 推挽输出 |
| DC | PD15 | 推挽输出 |
| CS | PD1 | 推挽输出 |
| BLK | PE8 | 推挽输出 |

这四根线不是 SPI1 自动管理的：

- `RES`：复位 LCD 控制器；
- `DC`：区分当前字节是命令还是数据；
- `CS`：选择 LCD；
- `BLK`：控制背光。

CubeMX 在 `main.h` 中生成了名称：

```c
#define LCD_BLK_Pin       GPIO_PIN_8
#define LCD_BLK_GPIO_Port GPIOE
#define LCD_DC_Pin        GPIO_PIN_15
#define LCD_DC_GPIO_Port  GPIOD
#define LCD_CS_Pin        GPIO_PIN_1
#define LCD_CS_GPIO_Port  GPIOD
#define LCD_RES_Pin       GPIO_PIN_4
#define LCD_RES_GPIO_Port GPIOD
```

使用这些名字比在 LCD 驱动中再次写死端口更可靠：以后在 CubeMX 中换引脚，只需要以新生成的宏为准。

---

## 5. 最少需要迁移哪些文件

从官方例程中保留并加入当前工程 `code` 目录的文件为：

```text
code/
├── lcd.c
├── lcd.h
├── lcd_init.c
├── lcd_init.h
├── lcdfont.h
└── types.h       ← 当前工程新增的兼容类型头文件
```

各文件职责如下：

| 文件 | 职责 | 迁移处理 |
|---|---|---|
| `lcd_init.c` | 字节发送、命令/数据发送、地址设置、ST7796 初始化 | 重点修改 |
| `lcd_init.h` | 引脚宏、屏幕尺寸、底层接口声明 | 重点修改 |
| `lcd.c` | 清屏、画点、画线、字符、数字和图片 | 基本保留，删除旧 `delay.h` 依赖 |
| `lcd.h` | 绘图接口与颜色定义 | 将 `sys.h` 改为 `types.h` |
| `lcdfont.h` | ASCII/汉字点阵数据 | 原样保留 |
| `types.h` | `u8/u16/u32` 兼容类型 | 新增 |

不需要把官方工程的整个 `USER`、`SYSTEM`、`LED`、启动文件、时钟代码和 Keil 工程复制过来。那些内容属于原 STM32F407ZG 工程底层，与当前 F407VET6 CubeMX 工程重复甚至冲突。

---

## 6. Keil 工程中如何加入这些文件

迁移文件存在磁盘上还不够，Keil 必须知道：

1. 哪些 `.c` 文件需要参与编译；
2. 编译器到哪里寻找 `.h` 文件。

最终工程在 Keil 的 `code` Group 中登记了：

```text
lcd.c
lcd.h
lcd_init.c
lcd_init.h
lcdfont.h
types.h
```

并在头文件搜索路径中加入：

```text
../code
```

其中真正参与编译的是 `.c` 文件；`.h` 文件加入 Group 主要是为了方便查看和管理。只把头文件显示在 Keil 左侧列表中，并不等于相应 `.c` 已经参与构建。

---

## 7. 第一步修改：建立兼容类型 `types.h`

官方代码大量使用：

```c
u8
u16
u32
```

这些不是 C 语言标准类型，而是原例程的 `sys.h` 提供的别名。当前工程不应为了三个类型把整套旧 `sys.h` 搬进来，因此新增：

```c
#ifndef _types_h
#define _types_h

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#endif
```

为什么这样改：

- `uint8_t` 明确为 8 位无符号整数；
- `uint16_t` 明确为 16 位无符号整数；
- `uint32_t` 明确为 32 位无符号整数；
- 保留 `u8/u16/u32` 后，官方绘图代码不需要逐个函数修改类型。

### 关于重复包含 `<stdint.h>`

当时曾担心 `headfile.h` 已包含 `<stdint.h>`，`types.h` 再包含一次会不会冲突。正常情况下没有问题，因为标准头文件有防重复包含保护。

真正需要避免的是在多个无关头文件中重复写互相冲突的 `typedef`，而不是重复 `#include <stdint.h>`。

---

## 8. 第二步修改：处理 `lcd.h`

官方 `lcd.h` 原来包含：

```c
#include "sys.h"
```

当前工程改成：

```c
#include "types.h"
```

原因是 `lcd.h` 实际只需要 `u8/u16/u32`，没有必要把原工程的寄存器、标准库和系统级定义一起带进当前 HAL 工程。

除了这个包含关系，`lcd.h` 中的绘图函数声明与颜色定义基本保留：

```c
void LCD_Fill(...);
void LCD_DrawPoint(...);
void LCD_DrawLine(...);
void LCD_DrawRectangle(...);
void Draw_Circle(...);
void LCD_ShowString(...);
void LCD_ShowIntNum(...);
void LCD_ShowPicture(...);
```

这些函数属于 LCD 上层绘图逻辑，与 GPIO 使用标准库还是 HAL 没有直接关系，因此不应无意义重写。

---

## 9. 第三步修改：处理 `lcdfont.h`

`lcdfont.h` 保存的是字体点阵数据。它不负责 GPIO，也不负责 SPI，因此本次迁移保持原样。

实际文件哈希对比表明，官方版本和最终工程中的 `lcdfont.h` 完全相同。

### 为什么不建议把 `lcdfont.h` 放进公共 `headfile.h`

字体头文件通常体积很大，而且有些厂家字库头文件直接定义数组，而不仅是 `extern` 声明。如果被很多 `.c` 文件重复包含，可能造成：

- 编译速度变慢；
- 每个源文件都解析大量点阵数据；
- 若头文件保护或定义方式不规范，可能出现重复定义；
- 公共头文件依赖越来越混乱。

因此最终保持：

```c
// 仅在 lcd.c 中包含
#include "lcdfont.h"
```

其他模块只包含 `lcd.h`，不直接接触字体数组。

---

## 10. 第四步修改：删除旧的 `LCD_GPIO_Init()`

官方 `lcd_init.c` 中的 `LCD_GPIO_Init()` 完成了：

```text
使能GPIOA/B/C/D/E/G时钟
配置PG12为输出
配置PD1/PD4/PD5/PD15为输出
配置PE8/PE10为输出
把相关引脚拉高
```

它使用的是标准外设库：

```c
RCC_AHB1PeriphClockCmd(...);
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
GPIO_Init(...);
GPIO_SetBits(...);
```

迁移时将整个 `LCD_GPIO_Init()` 删除，同时还要删除两个关联位置：

```c
// lcd_init.h 中删除
void LCD_GPIO_Init(void);

// LCD_Init() 中删除
LCD_GPIO_Init();
```

删除它并不是“不初始化 GPIO”，而是把初始化责任交还给 CubeMX：

```text
MX_GPIO_Init()
    └── 初始化RES/DC/CS/BLK

MX_SPI1_Init()
    └── 初始化SPI1及PA5/PA7复用功能
```

这样只有一个底层配置来源，避免 LCD 驱动在运行时又把 CubeMX 已配置好的复用引脚改回普通输出。

### 删除函数时必须检查三个位置

```text
函数定义
头文件声明
所有调用点
```

只删除其中一个位置会留下隐式声明、未定义引用或者无效接口。

---

## 11. 第五步修改：删除软件模拟 SCLK 和 MOSI

硬件 SPI 启用后，以下宏全部删除：

```c
LCD_SCLK_Clr()
LCD_SCLK_Set()
LCD_MOSI_Clr()
LCD_MOSI_Set()
```

原因：

- 原宏操作 PG12 和 PD5；
- 当前 SPI1 实际使用 PA5 和 PA7；
- PA5/PA7 已经配置为 `GPIO_AF5_SPI1`；
- SCK 与 MOSI 波形由 SPI1 外设自动产生；
- 不能一边让 SPI 外设控制，一边又用普通 GPIO 代码手动控制。

这里应当区分两类引脚：

```text
硬件SPI信号：SCK、MOSI
    → 由SPI1外设控制

LCD额外控制线：RES、DC、CS、BLK
    → 继续由HAL_GPIO_WritePin控制
```

因此不能把 LCD 的所有引脚宏全部删除。

---

## 12. 第六步修改：将控制线宏改成 HAL 写法

原来的标准库宏：

```c
#define LCD_RES_Clr() GPIO_ResetBits(GPIOD, GPIO_Pin_4)
#define LCD_RES_Set() GPIO_SetBits(GPIOD, GPIO_Pin_4)
```

改成使用 CubeMX 名称的 HAL 宏：

```c
#define LCD_RES_Clr()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_RESET)
#define LCD_RES_Set()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin, GPIO_PIN_SET)

#define LCD_DC_Clr()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_DC_Set()   HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

#define LCD_CS_Clr()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_Set()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)

#define LCD_BLK_Clr()  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_RESET)
#define LCD_BLK_Set()  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_SET)
```

为了得到 HAL 声明和 CubeMX 引脚宏，`lcd_init.h` 包含：

```c
#include "types.h"
#include "main.h"
```

### 当时出现过的重复宏问题

迁移中曾出现“前面已经增加 HAL 宏，文件后面还残留旧标准库同名宏”的情况：

```c
#define LCD_RES_Clr() HAL_GPIO_WritePin(...)
...
#define LCD_RES_Clr() GPIO_ResetBits(...)
```

后定义的旧宏可能覆盖前面的新宏，并产生重复定义警告。检查时应搜索并删除所有仍带这些特征的旧块：

```text
GPIO_SetBits
GPIO_ResetBits
GPIO_Pin_x
LCD_SCLK_
LCD_MOSI_
```

---

## 13. 第七步修改：把软件模拟发送替换成 `HAL_SPI_Transmit()`

官方版本：

```c
void LCD_Writ_Bus(u8 dat)
{
    u8 i;
    LCD_CS_Clr();
    for (i = 0; i < 8; i++)
    {
        LCD_SCLK_Clr();
        if (dat & 0x80)
            LCD_MOSI_Set();
        else
            LCD_MOSI_Clr();
        LCD_SCLK_Set();
        dat <<= 1;
    }
    LCD_CS_Set();
}
```

最终版本：

```c
#include "lcd_init.h"
#include "spi.h"

void LCD_Writ_Bus(u8 dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, HAL_MAX_DELAY);
}
```

四个参数分别表示：

| 参数 | 含义 |
|---|---|
| `&hspi1` | 使用 CubeMX 创建的 SPI1 句柄 |
| `&dat` | 发送数据在内存中的地址 |
| `1` | 发送一个 8 位字节 |
| `HAL_MAX_DELAY` | 阻塞等待发送完成，不使用普通超时 |

必须包含 `spi.h`，否则当前 `.c` 文件看不到 `hspi1` 的声明。

### 为什么只改这一个底层函数就够了

上层函数已经形成调用链：

```text
LCD_WR_REG
LCD_WR_DATA8
LCD_WR_DATA
LCD_Address_Set
LCD_Fill/LCD_ShowString/其他绘图函数
        ↓
LCD_Writ_Bus
```

只要 `LCD_Writ_Bus()` 从软件模拟改为硬件 SPI，绝大多数上层函数就自动完成迁移。这是比“把每一个绘图函数全部重写”更稳、更容易验证的方式。

---

## 14. 第八步修改：延时函数改为 HAL

官方代码使用：

```c
delay_ms(100);
```

当前工程改为：

```c
HAL_Delay(100);
```

并删除：

```c
#include "delay.h"
```

最终 LCD 复位和唤醒顺序中保留了必要等待：

```c
LCD_RES_Clr();
HAL_Delay(100);
LCD_RES_Set();
HAL_Delay(200);

LCD_BLK_Set();
HAL_Delay(100);

LCD_WR_REG(0x11);   // Sleep Out
HAL_Delay(120);
```

为什么不能把这些延时全部删除：屏幕复位、退出睡眠和内部电源稳定需要时间；SPI 传输完成不等于 LCD 内部状态已经准备好。

`HAL_Delay()` 依赖 HAL 的 SysTick，因此必须在 `HAL_Init()` 之后使用。

---

## 15. 第九步修改：处理 CS 和 DC

### 15.1 DC 的作用

ST7796 共用同一根 MOSI 接收命令和参数，DC 决定当前字节的性质：

```text
DC = 0：命令
DC = 1：参数或像素数据
```

因此 `LCD_WR_REG()` 保留：

```c
void LCD_WR_REG(u8 dat)
{
    LCD_DC_Clr();
    LCD_Writ_Bus(dat);
    LCD_DC_Set();
}
```

### 15.2 CS 的作用

CS 低电平表示 LCD 被选中：

```text
CS = 1：LCD通常忽略SPI数据
CS = 0：LCD接收SPI数据
```

原软件 SPI 每发送一个字节都会拉低、再拉高 CS。改成硬件 SPI 后，最终基础版本选择在 `LCD_Init()` 开始时拉低 CS，并在专用 SPI 总线上保持选中：

```c
void LCD_Init(void)
{
    LCD_CS_Clr();
    LCD_DC_Set();
    ...
}
```

这在当时 SPI1 只连接 LCD 的情况下可行，并已经实机验证。

如果以后让 LCD 与触控芯片等设备共用 SPI1，就必须为每次事务分别管理各自的 CS，确保同一时间只选中一个设备。

---

## 16. 为什么 ST7796 的初始化寄存器序列没有重写

`LCD_Init()` 中从 `0x11`、`0xF0`、`0x36`、`0x3A` 到伽马参数和 `0x29` 的命令，是针对 ST7796 控制器的初始化流程。

这部分与 STM32 使用标准库还是 HAL 无关：

```text
标准库/软件SPI 和 HAL/硬件SPI
只改变“字节如何送到屏幕”

ST7796寄存器序列
决定“屏幕收到字节后如何初始化自身”
```

因此迁移时保留原序列，只替换：

- GPIO 初始化；
- 字节发送方式；
- 延时接口；
- CS 的基础控制。

这种“尽可能保留已验证的设备协议层，只替换 MCU 适配层”的方法能显著减少变量。

---

## 17. `lcd.c` 实际改了什么

哈希与差异对比表明，最终验证版本的 `lcd.c` 相对官方文件只删除了：

```c
#include "delay.h"
```

清屏、画点、画线、字符、数字和图片函数主体均保留。

例如基础清屏函数仍然是逐像素版本：

```c
void LCD_Fill(u16 xsta, u16 ysta, u16 xend, u16 yend, u16 color)
{
    u16 i, j;
    LCD_Address_Set(xsta, ysta, xend - 1, yend - 1);
    for (i = ysta; i < yend; i++)
    {
        for (j = xsta; j < xend; j++)
        {
            LCD_WR_DATA(color);
        }
    }
}
```

它速度不是最快，但调用链清晰，而且是已经实机验证的可靠基线。

后续可以把同色像素打包后批量调用 `HAL_SPI_Transmit()`，但优化会引入新的变量，例如：

- 缓冲区字节顺序；
- SPI 超时；
- CS/DC 状态；
- HAL 返回值；
- SPI 被其他设备共用时的互斥；
- 一次传输长度和刷新耗时。

因此正确顺序应当是：

```text
先让原始逐像素版本稳定显示
→ 再单独优化LCD_Fill
→ 每次优化后做单色清屏验证
→ 最后再接入复杂UI
```

不能在白屏时同时怀疑初始化、接线和复杂 UI，而不先回到这份可用基线。

---

## 18. 第十步修改：在 `main.c` 中真正调用 LCD

仅仅把驱动文件放进工程并不能显示。最终验证工程的初始化顺序为：

```c
HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_CAN1_Init();
MX_USART2_UART_Init();
MX_SPI1_Init();

LCD_Init();
LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
LCD_ShowString(20, 20,
               (const u8 *)"CAN TEST",
               WHITE, BLACK, 32, 0);
```

关键依赖顺序为：

```text
HAL_Init
→ SystemClock_Config
→ MX_GPIO_Init
→ MX_SPI1_Init
→ LCD_Init
→ LCD_Fill/LCD_ShowString
```

原因：

1. `HAL_Init()` 建立 HAL 和 SysTick；
2. `SystemClock_Config()` 确定系统和 SPI 外设时钟；
3. `MX_GPIO_Init()` 配置 RES/DC/CS/BLK；
4. `MX_SPI1_Init()` 配置 SPI1 和 PA5/PA7；
5. `LCD_Init()` 才能安全调用 GPIO、延时和 SPI；
6. 初始化完成后才能设置显示窗口并写入像素。

`CAN TEST` 测试非常重要，因为它只验证：

```text
程序运行
GPIO正确
SPI正确
LCD初始化正确
字体显示调用链正确
```

它不依赖 CAN 数据，故障范围比一开始就显示动态 CAN 报文小得多。

---

## 19. 最终代码调用链

### 19.1 初始化调用链

```text
main()
 ├─ HAL_Init()
 ├─ SystemClock_Config()
 ├─ MX_GPIO_Init()
 ├─ MX_SPI1_Init()
 └─ LCD_Init()
      ├─ LCD_CS_Clr()
      ├─ LCD_RES_Clr()/Set()
      ├─ LCD_BLK_Set()
      └─ LCD_WR_REG()/LCD_WR_DATA8()
```

### 19.2 显示字符串调用链

```text
LCD_ShowString()
  ↓
LCD_ShowChar()
  ↓
读取lcdfont.h中的点阵
  ↓
LCD_WR_DATA()
  ↓
LCD_Writ_Bus()
  ↓
HAL_SPI_Transmit(&hspi1, ...)
  ↓
SPI1产生PA5时钟、从PA7发送数据
  ↓
ST7796写入显存
```

### 19.3 命令和像素数据的区别

```text
LCD_WR_REG()
    → DC拉低
    → 发送命令字节
    → DC恢复拉高

LCD_WR_DATA8()/LCD_WR_DATA()
    → DC保持高
    → 发送参数或RGB565像素数据
```

---

## 20. 迁移过程中实际遇到的错误与判断方法

### 20.1 大量标准库符号未定义

典型报错：

```text
RCC_AHB1PeriphClockCmd 未定义
RCC_AHB1Periph_GPIOx 未定义
GPIO_SetBits/GPIO_ResetBits 隐式声明
GPIO_Pin_15 未定义
GPIO_InitStructure.GPIO_Mode 不存在
```

原因：官方代码使用标准外设库，当前工程使用 HAL，两套 API 的类型和成员不能混用。

解决：删除旧 `LCD_GPIO_Init()`，让 CubeMX 负责底层 GPIO；控制线改用 `HAL_GPIO_WritePin()`。

### 20.2 误以为只改结构体类型名称就能完成迁移

把标准库结构体名称换成 HAL 名称还不够。标准库成员：

```c
GPIO_Pin
GPIO_Mode
GPIO_OType
GPIO_Speed
GPIO_PuPd
```

与 HAL 成员：

```c
Pin
Mode
Pull
Speed
Alternate
```

并不相同。更重要的是，这段 GPIO 初始化本来就不应继续存在，因为 CubeMX 已经生成正确底层。

### 20.3 删除函数后仍有声明或调用

当 `LCD_GPIO_Init()` 函数体已删除，但头文件声明或 `LCD_Init()` 调用仍在，会继续报错或留下混乱接口。

解决：同时检查定义、声明、调用。

### 20.4 新旧宏并存

HAL 宏和标准库宏同时存在时，看起来“已经改了”，但预处理器可能仍使用文件后面的旧宏。

解决：全文件搜索旧关键字，而不是只看顶部几行。

### 20.5 把硬件 SPI 与普通控制线混淆

错误做法包括：

- 继续手动翻转 SCK/MOSI；
- 把 RES/DC/CS/BLK 也当成 SPI 自动信号；
- 没有配置 DC，却期望屏幕自动区分命令和数据。

正确分工：

```text
SPI1：SCK、MOSI
普通GPIO：RES、DC、CS、BLK
```

### 20.6 CS 初始为高但没有选中屏幕

CubeMX 中 CS 初始设置为高是安全的，但在发送屏幕数据前必须拉低。否则 SPI 外设可以正常产生波形，LCD 仍然不会响应。

### 20.7 `types.h` 重复包含 `<stdint.h>` 的误解

重复包含标准头文件通常没有问题。问题重点应放在：

- 是否有 include guard；
- typedef 是否只定义一次；
- 类型宽度是否明确。

### 20.8 `lcdfont.h` 不适合塞进所有公共头文件

它属于 LCD 内部数据依赖。只在 `lcd.c` 中包含可以减少耦合，也避免大字库被到处解析。

### 20.9 Keil 编辑器改了但磁盘文件没有更新

当标签页带 `*` 时，内容可能尚未保存。磁盘检查与编辑器画面不一致时，应先 `Ctrl+S`，再重新编译或重新读取。

### 20.10 注释乱码

部分厂商文件使用 GBK/ANSI，当前工具按 UTF-8 读取时会显示乱码。乱码注释通常不影响 C 代码执行，但会严重影响学习和维护。

处理原则：

- 不为了修注释随意改动大段已验证代码；
- 需要统一编码时单独处理，并确认没有改变字库二进制数据；
- 优先保证代码行为正确，再整理注释。

### 20.11 编译成功不等于屏幕一定显示

屏幕显示链路为：

```text
程序是否运行
→ 系统时钟
→ GPIO初始化
→ SPI1初始化
→ SCK/MOSI接线
→ CS/DC/RES/BLK接线和电平
→ ST7796初始化序列
→ 显存写入
→ 背光
```

编译只证明语法与链接通过，不证明每一层硬件链路正确。

---

## 21. 当时最合理的逐步验证方法

### 阶段一：只验证背光和复位

确认：

- BLK 能打开；
- RES 有低—高复位过程；
- 屏幕供电和地线正确。

背光亮只代表背光电路工作，不代表 ST7796 收到了初始化命令。

### 阶段二：单色清屏

```c
LCD_Init();
LCD_Fill(0, 0, LCD_W, LCD_H, RED);
```

若整屏颜色正确，说明初始化、窗口设置和像素发送基本成立。

### 阶段三：固定英文字符串

```c
LCD_ShowString(20, 20,
               (const u8 *)"CAN TEST",
               WHITE, BLACK, 32, 0);
```

成功后说明字体点阵和字符绘制调用链也成立。

### 阶段四：再接 CAN 数据

只有固定显示验证成功后，才把 CAN 接收结果写到屏幕。这样如果失败，可以区分：

```text
LCD本身失败
还是CAN没有收到数据
还是动态刷新逻辑有问题
```

---

## 22. 如何在屏幕上显示 CAN 接收结果

屏幕接口最终提供了两类常用能力：

```c
LCD_ShowString(...);  // 固定文字
LCD_ShowIntNum(...);  // 整数
```

CAN 数据通常需要先整理成字符串，再刷新指定区域。例如思路为：

```c
char line[32];

snprintf(line, sizeof(line), "ID:%03lX", rx_id);
LCD_ShowString(10, 60,
               (const u8 *)line,
               WHITE, BLACK, 16, 0);
```

动态刷新前最好先用背景色覆盖旧内容，或者始终输出固定宽度文本，否则新字符串比旧字符串短时会留下残影。

学习顺序应当是：

```text
固定字符串
→ 固定数字
→ CAN ID和DLC
→ 8字节数据
→ 最后再做界面布局和高频刷新
```

---

## 23. 为什么硬件 SPI 更适合当前项目

### 软件模拟 SPI

优点：

- 任意普通 GPIO 都能使用；
- 波形逻辑直观；
- 适合入门观察。

缺点：

- CPU 需要逐位翻转 GPIO；
- 刷新大尺寸彩屏时速度慢；
- 软件延时和指令执行时间影响时序；
- 与 CAN、串口、触控等任务同时运行时，占用更多 CPU。

### 硬件 SPI

优点：

- SPI 外设自动产生时钟和数据；
- 速度更稳定；
- CPU 负担更低；
- 后续可继续使用中断或 DMA 优化；
- 更适合 320×480 彩屏和动态仪表界面。

缺点：

- 必须使用 MCU 支持相应 AF 的引脚；
- CPOL、CPHA、分频配置错误时可能完全不显示；
- 多设备共用总线时必须认真管理 CS 和不同设备的时序模式。

因此对当前“CAN 接收 + USART 调试 + 彩屏显示”的项目，硬件 SPI 是更合适的长期方案。

---

## 24. 复制到后续工程时的检查清单

### 24.1 文件

- [ ] `lcd.c` 已加入 Keil 编译；
- [ ] `lcd_init.c` 已加入 Keil 编译；
- [ ] `../code` 已加入 Include Path；
- [ ] `lcd.h/lcd_init.h/lcdfont.h/types.h` 位于可访问路径；
- [ ] 没有把原工程的启动文件、`sys.c`、`delay.c` 整套复制进来。

### 24.2 CubeMX

- [ ] SPI1 为 Master；
- [ ] 数据位为 8 bit；
- [ ] MSB First；
- [ ] CPOL/CPHA 与实机可用版本一致；
- [ ] NSS 由软件管理；
- [ ] SCK/MOSI 使用正确 AF；
- [ ] RES/DC/CS/BLK 为推挽输出；
- [ ] 没有引脚冲突。

### 24.3 代码

- [ ] `MX_GPIO_Init()` 在 `LCD_Init()` 前；
- [ ] `MX_SPI1_Init()` 在 `LCD_Init()` 前；
- [ ] `LCD_Writ_Bus()` 使用正确的 SPI 句柄；
- [ ] `LCD_Init()` 发送数据前已经选中 LCD；
- [ ] 没有残留 `GPIO_SetBits/GPIO_ResetBits`；
- [ ] 没有残留 `LCD_SCLK_*` 和 `LCD_MOSI_*` 软件模拟宏；
- [ ] 没有残留对旧 `delay.h`、`sys.h` 的依赖。

### 24.4 硬件

- [ ] VCC 与模块要求一致；
- [ ] GND 共地；
- [ ] SCK/MOSI 没接反；
- [ ] CS/DC/RES/BLK 与当前工程宏一致；
- [ ] 背光亮与“LCD控制器完成初始化”分开判断；
- [ ] 先做单色和固定字符串测试，再接入 CAN。

---

## 25. 最终迁移结果总结

这次迁移最终形成的职责划分是：

```text
CubeMX / HAL
├── 系统时钟
├── GPIO端口时钟
├── PA5/PA7复用功能
├── RES/DC/CS/BLK输出配置
└── SPI1参数及hspi1句柄

ST7796驱动
├── LCD复位与背光控制
├── 命令/数据区分
├── ST7796寄存器初始化
├── 显示窗口设置
├── RGB565像素写入
└── 字符、数字、图形绘制
```

最关键的迁移思想是：

```text
不要机械复制整个官方工程；
保留已经验证的屏幕协议与绘图逻辑；
用目标工程自己的CubeMX底层替换旧标准库和软件模拟接口；
先完成最小显示验证，再逐步接入CAN、触控和复杂UI。
```

最终实机成功显示 `CAN TEST`，证明这条基础链路是可靠的。以后屏幕出现白屏或复杂界面不显示时，应首先回到这份基础版本做对照，而不是同时修改 SPI、LCD 初始化、CAN 和 UI。
