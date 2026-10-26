/*********************************************************************************
 * @file    wrap.c
 * @brief   CAN 总线底层应用封装层 (电赛/工业级高可靠通信接口)
 * 
 * ================================================================
 * 模块架构总览：
 *   1. can_filter_init()   : 硬件 32 位全通掩码滤波器配置与 CAN1 控制器启动
 *   2. can_send_message()  : 标准数据帧硬件邮箱快速装载与非阻塞发送
 *   3. can_get_message()   : FIFO0 接收深度轮询检测与零拷贝报文提取
 * 
 * 硬件外设与通信拓扑：
 *   - 控制器   : STM32F407VET6 CAN1 (主控制器)
 *   - 引脚映射 : CAN1_RX -> PB8 (AF9), CAN1_TX -> PB9 (AF9)
 *   - 收发器   : VP230 / TJA1050 (3.3V/5V 工业级 CAN 收发芯片)
 *   - 总线标准 : 500 kbps, 12 TQ, 采样点 83.33%, 终端并联 120Ω 匹配电阻
 * ================================================================
 *********************************************************************************/
#include "headfile.h"

// ================================================================
// 1. can_filter_init: CAN1 硬件验收滤波器全通初始化
//
//    硬件资源与双 CAN 架构原理：
//      - STM32F407 共有 28 组硬件滤波器 (FilterBank 0 ~ 27)，由 CAN1 和 CAN2 共享。
//      - SlaveStartFilterBank = 14: 规定 0~13 号 FilterBank 归属于 CAN1，
//        14~27 号归属于 CAN2。即便不使能 CAN2，此边界配置也是硬件初始化的必要条件。
//
//    32 位全通掩码模式数学逻辑：
//      - 匹配规则：Received_ID & Mask == Filter_ID & Mask
//      - 当 MaskIdHigh = 0x0000 且 MaskIdLow = 0x0000 时，所有比特位均为“无关位 (Don't Care)”。
//      - 效果：不过滤任何报文，无条件接收总线上所有标准数据帧、扩展帧、广播帧，
//        将其全部压入硬件 FIFO0 队列中，为多节点网络诊断提供全透明监听链路。
//
//    ⚠️ 避坑指南：
//      - HAL_CAN_ConfigFilter 必须在 HAL_CAN_Start() 之前执行，否则寄存器锁定报错！
//      - 必须显式激活 FilterActivation = CAN_FILTER_ENABLE，否则硬件过滤器保持旁路丢包。
// ================================================================
void can_filter_init(void)
{
    CAN_FilterTypeDef Can_ConfigFilter = {0};

    // ------- 1. 滤波器物理硬件绑定 -------
    Can_ConfigFilter.FilterActivation     = CAN_FILTER_ENABLE;       // 激活该组硬件过滤器
    Can_ConfigFilter.FilterBank           = 0;                       // 选用 CAN1 第 0 组滤波器
    Can_ConfigFilter.FilterFIFOAssignment = CAN_FILTER_FIFO0;        // 匹配命中的报文自动推入 FIFO0
    Can_ConfigFilter.SlaveStartFilterBank = 14;                      // CAN2 从属滤波器起始号 (硬分界点)

    // ------- 2. 掩码与验证 ID 配置 (全通模式) -------
    Can_ConfigFilter.FilterMode           = CAN_FILTERMODE_IDMASK;   // 标识符屏蔽位模式 (ID+Mask)
    Can_ConfigFilter.FilterScale          = CAN_FILTERSCALE_32BIT;    // 32 位单组宽模式 (精准覆盖扩展ID)
    Can_ConfigFilter.FilterIdHigh         = 0x0000;                  // 验证码高 16 位
    Can_ConfigFilter.FilterIdLow          = 0x0000;                  // 验证码低 16 位
    Can_ConfigFilter.FilterMaskIdHigh     = 0x0000;                  // 屏蔽码高 16 位 (全 0: 不屏蔽任何位)
    Can_ConfigFilter.FilterMaskIdLow      = 0x0000;                  // 屏蔽码低 16 位 (全 0: 允许所有帧通过)

    // ------- 3. 写入硬件寄存器并校验状态 -------
    if (HAL_CAN_ConfigFilter(&hcan1, &Can_ConfigFilter) != HAL_OK)
    {
        printf("filter error\r\n");
        Error_Handler();
    }
    printf("filter ok\r\n");

    // ------- 4. 退出初始化模式，正式启动 CAN1 控制器 -------
    HAL_CAN_Start(&hcan1);
}

// ================================================================
// 2. can_send_message: CAN 报文发送接口 (标准数据帧)
//
//    参数说明：
//      - tx_stdid : 标准帧 11 位标识符 (例如仪表方向盘测试帧 0x123)
//      - dlc_len  : 数据长度码 (Data Length Code, 范围 0~8 字节)
//      - data     : 指向待发送字节数组的指针
//
//    ★ 深度认知 (发送成功的真正标志)：
//      - HAL_CAN_AddTxMessage 返回 HAL_OK，仅代表该帧已被成功推入 STM32 发送邮箱 (Tx Mailbox 0/1/2)。
//      - 报文是否真正发射到物理差分总线上、是否有其他节点在 ACK 槽拉低确认，
//        需要通过硬件发送状态寄存器 (TSR 的 TXOK 位) 进行判定。
// ================================================================
void can_send_message(uint32_t tx_stdid, uint32_t dlc_len, uint8_t* data)
{
    CAN_TxHeaderTypeDef TxMessage = {0};
    uint32_t TxMailbox;

    // ------- 1. 构建标准 CAN 报文头结构体 -------
    TxMessage.StdId = tx_stdid;          // 标准 11 位标识符
    TxMessage.IDE   = CAN_ID_STD;        // 标识符扩展标志：标准帧 (11 位 ID)
    TxMessage.DLC   = dlc_len;           // 数据有效载荷长度 (通常为 8 字节)
    TxMessage.RTR   = CAN_RTR_DATA;      // 帧类型：数据帧 (Data Frame, 非远程请求帧)

    // ------- 2. 请求空闲硬件发送邮箱并提交发送请求 -------
    HAL_CAN_AddTxMessage(&hcan1, &TxMessage, data, &TxMailbox);
}

// ================================================================
// 3. can_get_message: CAN1 报文非阻塞轮询读取接口
//
//    返回值：
//      - 0 : 成功从 FIFO0 读取到一帧有效报文
//      - 1 : 当前 FIFO0 为空，或者硬件读取异常
//
//    设计哲学 (非阻塞状态机)：
//      - 严禁在主循环中做阻塞等待，必须先调用 HAL_CAN_GetRxFifoFillLevel 探测深度；
//      - 若 FIFO 为空直接返回 1，耗时仅几纳秒，保障 LVGL 60FPS UI 渲染完全不卡顿！
// ================================================================
uint32_t can_get_message(uint32_t* rx_id, uint32_t* rx_len, uint8_t* rx_data)
{
    CAN_RxHeaderTypeDef Rxmessage = {0};

    // ------- 1. 硬件深度探测 (非阻塞安全屏障) -------
    // 若 FIFO0 中没有积压的报文，立即瞬时返回，不浪费 CPU 周期
    if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) == 0u)
    {
        return 1;
    }

    // ------- 2. 硬件 FIFO 提取并释放邮箱 -------
    // 成功读取后，硬件会自动释放 FIFO0 当前槽位，深度计数自动减 1
    if (HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &Rxmessage, rx_data) != HAL_OK)
    {
        printf("get_error\r\n");
        return 1;
    }

    // ------- 3. 协议字段提取 (标准 ID 与 DLC) -------
    *rx_id  = Rxmessage.StdId;
    *rx_len = Rxmessage.DLC;

    // ------- 4. 调试串口格式化输出 (方便逻辑分析仪与终端比对) -------
    printf("rx_ok ID=0x%03lX DLC=%lu data: ", (unsigned long)(*rx_id), (unsigned long)(*rx_len));
    for (uint32_t i = 0; i < *rx_len; i++)
    {
        printf("%02X ", rx_data[i]);
    }
    printf("\r\n");

    return 0; // 成功解析出有效物理报文
}
