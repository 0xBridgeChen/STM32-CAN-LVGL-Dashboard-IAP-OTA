# STM32F407 + ST7796：从标准库软件模拟 SPI 迁移到 CubeMX HAL 硬件 SPI

## 1. 这次迁移要解决什么问题

当前工程是：

- MCU：STM32F407VET6
- 工程框架：STM32CubeMX + HAL + Keil
- 屏幕控制器：ST7796
- 目标：保留官方例程中的屏幕初始化、绘图和字符显示逻辑，把底层的“标准库 + GPIO 软件模拟 SPI”改成当前工程使用的“CubeMX + HAL 硬件 SPI1”。

这不是把官方工程全部复制过来，而是分成两层处理：

```text
保留：ST7796 的寄存器初始化、地址设置、写命令、写数据、绘图和字符显示逻辑

替换：旧标准库 GPIO 初始化、GPIO_SetBits/GPIO_ResetBits、软件模拟 SCLK/MOSI
```

核心原则是：屏幕上层业务逻辑可以迁移，MCU 底层配置必须以当前 STM32F407VET6 的 CubeMX 工程为准。

---

## 2. 软件模拟 SPI 和硬件 SPI 分别是什么

### 2.1 软件模拟 SPI

原官方例程没有让 STM32 的 SPI 外设自动发送，而是把普通 GPIO 手动拉高、拉低，例如：

```c
LCD_SCLK_Clr();
LCD_MOSI_Set();
LCD_SCLK_Set();
```

程序需要重复这些操作八次，才能发送一个字节。

硬件上发生的过程是：

```text
CPU执行GPIO语句
→ 改变SCLK电平
→ 改变MOSI电平
→ 重复8次
→ 发送完1字节
```

优点是可以使用普通 GPIO，理解起来直观；缺点是速度较慢、占用 CPU，而且移植时需要手动改端口和引脚。

### 2.2 硬件 SPI

现在使用 STM32 内部的 SPI1 外设：

```c
HAL_SPI_Transmit(&hspi1, &dat, 1, HAL_MAX_DELAY);
```

程序只把一个字节交给 SPI1，SPI1 会自动产生时钟并从 MOSI 发出八位数据：

```text
CPU把数据交给SPI1
→ SPI1自动产生SCK
→ SPI1自动从MOSI发送8位数据
→ CPU不需要逐位操作GPIO
```

它和 CAN 的思路类似：应用代码把数据交给外设，外设负责协议要求的具体波形。`HAL_SPI_Transmit()` 表示数据已经交给 SPI 外设发送，但屏幕是否正确显示，仍取决于 SPI 模式、接线、CS/DC/RES 和屏幕初始化指令等条件。

---

## 3. 当前工程的实际 CubeMX 配置

以下信息来自当前工程的 `.ioc` 和生成代码。

### 3.1 SPI1 数据线

| 信号 | STM32F407 引脚 | 配置 |
|---|---:|---|
| SPI1_SCK | PA5 | GPIO_AF5_SPI1 |
| SPI1_MOSI | PA7 | GPIO_AF5_SPI1 |

SPI1 当前主要参数：

```text
主机模式（Master）
8位数据
高位先发（MSB First）
软件管理 NSS
CPOL = High
CPHA = 2Edge
分频 = 16
当前计算速率约 5.25 Mbit/s
```

PA5 和 PA7 已在 `HAL_SPI_MspInit()` 中配置成复用推挽输出，复用编号为 `GPIO_AF5_SPI1`。因此它们不应再由 LCD 驱动作为普通 GPIO 手动翻转。

### 3.2 LCD 控制线

| LCD 信号 | STM32F407 引脚 | 作用 |
|---|---:|---|
| RES | PD4 | 屏幕硬件复位 |
| DC | PD15 | 区分命令和数据 |
| CS | PD1 | 片选，低电平选中屏幕 |
| BLK | PE8 | 背光控制 |

这些引脚由 CubeMX 生成的 `MX_GPIO_Init()` 配置为普通推挽输出，因此 LCD 驱动只需要用 `HAL_GPIO_WritePin()`改变电平，不应该再次配置 GPIO。

---

## 4. 原代码为什么不能直接放进当前工程

原例程使用的是 STM32 标准外设库风格，存在这些典型名称：

```c
RCC_AHB1PeriphClockCmd(...)
RCC_AHB1Periph_GPIOA
GPIO_InitTypeDef GPIO_InitStructure
GPIO_InitStructure.GPIO_Pin
GPIO_InitStructure.GPIO_Mode
GPIO_Mode_OUT
GPIO_OType_PP
GPIO_Speed_100MHz
GPIO_PuPd_UP
GPIO_Init(...)
GPIO_SetBits(...)
GPIO_ResetBits(...)
GPIO_Pin_15
```

当前工程使用 HAL，等价接口的写法不同：

| 原标准库写法 | 当前 HAL/CubeMX 做法 |
|---|---|
| `RCC_AHB1PeriphClockCmd()` | CubeMX 生成 `__HAL_RCC_GPIOx_CLK_ENABLE()` |
| `GPIO_Init()` | CubeMX 生成 `HAL_GPIO_Init()` |
| `GPIO_SetBits()` | `HAL_GPIO_WritePin(..., GPIO_PIN_SET)` |
| `GPIO_ResetBits()` | `HAL_GPIO_WritePin(..., GPIO_PIN_RESET)` |
| `GPIO_Pin_15` | `GPIO_PIN_15` 或 CubeMX 生成的引脚名称 |

两套库虽然都控制同一个 STM32 外设，但类型、结构体成员、常量名称和函数名称并不兼容。把旧代码直接加入 HAL 工程，就会产生大量“未定义标识符”和“函数隐式声明”错误。

---

## 5. 实际迁移过程

### 第一步：解决基础类型

官方 LCD 代码使用了：

```c
u8
u16
u32
```

当前工程通过 `types.h` 将它们映射到标准定宽整数：

```c
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
```

`types.h` 包含 `<stdint.h>`，其他头文件再次间接包含 `<stdint.h>`通常没有问题，因为标准头文件和工程头文件都有防止重复包含的保护。真正需要避免的是同一个 typedef 在多个地方被重复、冲突地定义。

### 第二步：把旧延时改为 HAL 延时

原例程的：

```c
delay_ms(100);
```

改成：

```c
HAL_Delay(100);
```

当前 `LCD_Init()` 中使用了：

```c
HAL_Delay(100);
HAL_Delay(200);
HAL_Delay(100);
HAL_Delay(120);
```

`HAL_Delay()` 的参数单位是毫秒，并依赖 `HAL_Init()` 后工作的 SysTick 时基。因此必须在主程序完成 `HAL_Init()` 后再调用 LCD 初始化。

### 第三步：删除旧的 `LCD_GPIO_Init()`

原代码中的整个函数：

```c
void LCD_GPIO_Init(void)
{
    RCC_AHB1PeriphClockCmd(...);
    GPIO_InitStructure.GPIO_Pin = ...;
    GPIO_Init(...);
    GPIO_SetBits(...);
}
```

被删除，配套删除了：

```c
void LCD_GPIO_Init(void);
```

以及 `LCD_Init()` 中的：

```c
LCD_GPIO_Init();
```

删除的原因不是“不需要初始化 GPIO”，而是 GPIO 已经由 CubeMX 生成的 `MX_GPIO_Init()` 初始化。继续保留旧函数会同时造成：

1. 标准库接口无法在 HAL 工程中识别；
2. LCD 驱动重复配置 CubeMX 已配置的引脚；
3. 原工程的 SCLK/MOSI 引脚与当前硬件 SPI1 引脚不一致；
4. 后续重新生成 CubeMX 代码时，底层配置来源变得混乱。

正确职责划分是：

```text
MX_GPIO_Init()：初始化RES、DC、CS、BLK
MX_SPI1_Init()：初始化PA5 SCK、PA7 MOSI和SPI1参数
LCD_Init()：复位屏幕并发送ST7796初始化指令
```

### 第四步：把控制线宏改成 HAL 写法

当前 `lcd_init.h` 中的控制宏为：

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

使用 CubeMX 生成的名称而不是到处直接写 `GPIOD` 和 `GPIO_PIN_4`，可以让代码含义更清楚，也能降低以后换引脚时漏改的概率。

`lcd_init.h` 还包含：

```c
#include "main.h"
#include "types.h"
```

其中 `main.h` 提供 HAL 声明和 CubeMX 生成的 LCD 引脚宏，`types.h` 提供 `u8/u16/u32`。

### 第五步：删除软件模拟 SCLK 和 MOSI

原来的这些宏被删除：

```c
LCD_SCLK_Clr()
LCD_SCLK_Set()
LCD_MOSI_Clr()
LCD_MOSI_Set()
```

原例程中的 SCLK 和 MOSI 分别使用 PG12 和 PD5，而当前 CubeMX 硬件 SPI1 使用 PA5 和 PA7。硬件 SPI 启用后，PA5、PA7属于 SPI1 外设，不再由 LCD 驱动作为普通 GPIO 控制。

### 第六步：替换最底层的字节发送函数

原来的 `LCD_Writ_Bus()` 会循环八次，根据数据位手动改变 MOSI 和 SCLK。

现在改为：

```c
void LCD_Writ_Bus(u8 dat)
{
    HAL_SPI_Transmit(&hspi1, &dat, 1, HAL_MAX_DELAY);
}
```

参数含义：

| 参数 | 含义 |
|---|---|
| `&hspi1` | 使用 CubeMX 初始化的 SPI1 |
| `&dat` | 待发送字节的地址 |
| `1` | 发送一个字节 |
| `HAL_MAX_DELAY` | 阻塞等待发送完成，不设置普通超时 |

为了让 `hspi1` 对 `lcd_init.c` 可见，文件包含：

```c
#include "spi.h"
```

上层函数不用跟着重写：

```c
LCD_WR_REG()
LCD_WR_DATA8()
LCD_WR_DATA()
LCD_Address_Set()
```

这些函数最终都会调用 `LCD_Writ_Bus()`，因此只替换最底层发送函数，就能把上层所有写命令、写数据操作切换到硬件 SPI。

### 第七步：正确处理 CS 片选

CubeMX 当前把 `LCD_CS` 的初始电平设置为高：

```c
HAL_GPIO_WritePin(GPIOD, LCD_DC_Pin | LCD_CS_Pin | LCD_RES_Pin, GPIO_PIN_SET);
```

而 LCD 的 CS 是低电平有效：

```text
CS = 高：屏幕未选中，通常忽略SPI数据
CS = 低：屏幕被选中，接收SPI数据
```

因此在 `LCD_Init()` 开头加入：

```c
LCD_CS_Clr();
LCD_DC_Set();
```

当前 SPI1 只用于这一块屏幕，可以在初始化后让 CS 一直保持低电平。以后如果 SPI1 总线上还要挂接其他设备，就应当按一次完整事务控制各设备的 CS，不能让所有设备同时处于选中状态。

---

## 6. 迁移过程中实际遇到的问题

### 问题一：把标准库代码直接放进 HAL 工程

现象是 Keil 出现大量红线和错误，包括：

```text
RCC_AHB1PeriphClockCmd 无法识别
RCC_AHB1Periph_GPIOx 未定义
GPIO_SetBits/GPIO_ResetBits 被隐式声明
GPIO_Pin_x 未定义
旧 GPIO_InitStructure 成员无法识别
```

原因不是屏幕初始化指令错误，而是标准库 API 和 HAL API 混用。解决方式是保留屏幕协议逻辑，删除旧底层初始化并改用 CubeMX/HAL 接口。

### 问题二：只替换延时，但没有提供 HAL 声明

把 `delay_ms()` 改成 `HAL_Delay()` 后，还必须保证相关源码能看到 HAL 的声明。当前通过 `lcd_init.h` 包含 `main.h` 解决。

### 问题三：删除函数定义后，遗漏函数声明或调用

迁移过程中曾出现：

```text
LCD_GPIO_Init() 函数本体已删除
但 lcd_init.h 中声明仍存在
或 LCD_Init() 中仍调用 LCD_GPIO_Init()
```

删除一个不再使用的函数时，需要同时检查三个位置：

```text
函数定义
头文件声明
所有调用位置
```

### 问题四：新旧 GPIO 宏同时保留

曾经在 `lcd_init.h` 前面加入了 HAL 宏，但文件后面仍保留同名的旧标准库宏。

例如：

```c
#define LCD_RES_Clr() HAL_GPIO_WritePin(...)
```

后面又出现：

```c
#define LCD_RES_Clr() GPIO_ResetBits(...)
```

这不仅会产生宏重复定义警告，而且后出现的旧宏会覆盖前面的新宏，导致程序实际上仍使用旧接口。解决方法是完整删除含有以下特征的旧宏块：

```text
GPIO_SetBits
GPIO_ResetBits
GPIO_Pin_x
```

### 问题五：把 SCLK/MOSI 和普通控制线混为一类

硬件 SPI 迁移后：

```text
SCLK、MOSI：由SPI1外设自动控制
RES、DC、CS、BLK：仍由普通GPIO控制
```

不能把所有 LCD 引脚宏全部删除。只应删除软件模拟的 SCLK/MOSI 操作，保留四条额外控制线。

### 问题六：CS 默认高，却没有在发送前选中屏幕

即使 `HAL_SPI_Transmit()` 正常执行，如果 CS 一直为高，屏幕也可能完全不响应。最终在 `LCD_Init()` 最前面使用 `LCD_CS_Clr()` 选中屏幕。

### 问题七：编辑器内容和磁盘文件可能不同

有一次编辑器里看起来已经删除了代码，但重新读取磁盘文件时，旧声明或旧宏仍存在。这通常意味着文件尚未保存。每完成一步，应确认 Keil 标签页上的 `*` 消失，或者按 `Ctrl+S` 保存后再检查。

### 问题八：注释乱码和旧注释

当前部分中文注释显示乱码，这是源文件编码和 Keil 当前解码方式不一致造成的。它通常不影响 C 代码执行，但影响阅读。

另外，`LCD_Writ_Bus()` 已经改为硬件 SPI，但头文件注释仍可能写着“模拟 SPI 时钟”。这属于注释不准确，不影响程序运行，可以改为：

```c
void LCD_Writ_Bus(u8 dat);  // 硬件SPI发送一个字节
```

---

## 7. 当前底层调用关系

迁移后的屏幕发送路径为：

```text
LCD_Init()/绘图或字符函数
        ↓
LCD_WR_REG() 或 LCD_WR_DATA8()/LCD_WR_DATA()
        ↓
LCD_Writ_Bus()
        ↓
HAL_SPI_Transmit(&hspi1, ...)
        ↓
STM32 SPI1 外设
        ↓
PA5输出SCK，PA7输出MOSI
        ↓
ST7796接收数据
```

命令和数据通过 DC 区分：

```text
LCD_DC_Clr()：接下来发送的是命令
LCD_DC_Set()：接下来发送的是参数或像素数据
```

复位、片选和背光分别由 RES、CS、BLK 控制。

---

## 8. 正确的初始化顺序

最终主程序中应该保证：

```c
HAL_Init();
SystemClock_Config();

MX_GPIO_Init();
MX_SPI1_Init();

LCD_Init();
```

顺序原因：

1. `HAL_Init()` 先建立 HAL 和 SysTick 基础；
2. `SystemClock_Config()` 建立系统和外设时钟；
3. `MX_GPIO_Init()` 初始化 RES/DC/CS/BLK；
4. `MX_SPI1_Init()` 初始化 SPI1、PA5 和 PA7；
5. `LCD_Init()` 才能使用 GPIO、`HAL_Delay()` 和 `HAL_SPI_Transmit()`。

当前磁盘上的 `main.c` 已调用 `MX_GPIO_Init()` 和 `MX_SPI1_Init()`，但尚未看到 `LCD_Init()` 的调用。因此 LCD 底层迁移完成并不等于屏幕已经会显示；后续需要在合适的 USER CODE 区包含 LCD 头文件并调用 `LCD_Init()`。

---

## 9. 当前已经完成与尚未完成的范围

### 已完成

- CubeMX 已配置 SPI1；
- PA5 已配置为 SPI1_SCK；
- PA7 已配置为 SPI1_MOSI；
- RES/DC/CS/BLK 已由 CubeMX 配置为 GPIO 输出；
- 旧标准库 `LCD_GPIO_Init()` 已移除；
- 旧软件模拟 SCLK/MOSI 宏已移除；
- GPIO 控制宏已改为 `HAL_GPIO_WritePin()`；
- `delay_ms()` 已改成 `HAL_Delay()`；
- `LCD_Writ_Bus()` 已改成 `HAL_SPI_Transmit()`；
- `LCD_Init()` 开头已经拉低 CS，并设置默认 DC 状态；
- ST7796 原有寄存器初始化序列得到保留。

### 尚未完成或尚未验证

- 尚未系统检查 `lcd.c` 和 `lcd.h` 中是否还有旧标准库接口；
- 尚未确认 `lcd.c` 中的清屏、画点、字符显示函数全部适配；
- 当前 `main.c` 尚未看到 `LCD_Init()` 调用；
- 尚未加入最小显示测试，例如清屏并显示 `CAN TEST`；
- 本阶段没有运行 Keil 编译；
- 尚未通过真实屏幕验证 SPI 模式、接线、供电和显示方向；
- `HAL_SPI_Transmit()` 当前没有检查返回值，基础显示跑通后可根据需要增加错误处理。

因此目前准确结论是：

```text
lcd_init.c/lcd_init.h 的底层发送方式已经从标准库软件SPI迁移为HAL硬件SPI1；
整个LCD驱动和真实硬件显示还没有完成最终验证。
```

---

## 10. 下一步建议

继续保持一次只处理一层：

```text
1. 检查 lcd.c 和 lcd.h 是否仍含旧标准库接口
2. 确认 LCD_Clear、LCD_DrawPoint、LCD_ShowString 等函数的调用链
3. 在 main.c 的 USER CODE 区调用 LCD_Init()
4. 先只做单色清屏测试
5. 清屏成功后显示固定字符串 CAN TEST
6. 最后再把 CAN 接收数据接入屏幕显示
```

不要一开始就把 CAN 接收、串口打印、LCD 刷屏全部混在一起测试。先确认 LCD 自己能初始化和显示固定内容，再与已经工作的 CAN 接收代码结合，这样出现故障时才能明确是 LCD 层还是 CAN 层。
