#include <Keyboard.h> // 包含键盘库，这是实现键盘模拟的核心[5](@ref)
#include <Mouse.h>
//6668 5b81
// 定义串口通信的波特率
#define SERIAL_BAUD_RATE 9600

char buffer[100];
int pr = 0;

enum pasreDataStatus
{
    findHeadFirtst,
	findHeadSecond,
    findEndFirtst,
	findEndSecond,
};
pasreDataStatus status = findHeadFirtst;
uint16_t crc_16(uint8_t *data, uint16_t len)
{
    uint16_t crc_reg = 0xffff;
    for (uint16_t i = 0; i < len; i++)
    {
        //infof(" crc_16{:x} i {}", data[i],i);
        crc_reg ^= data[i];
        for (uint16_t j = 0; j < 8; j++)
        {
            if (crc_reg & 0x01)
            {
                crc_reg = ((crc_reg >> 1) ^ 0xa001);
            }
            else
            {
                crc_reg = crc_reg >> 1;
            }
        }
    }
    return crc_reg;
}



void setup() {
  // 初始化串口通信，用于接收来自电脑的指令[1](@ref)
  Serial.begin(SERIAL_BAUD_RATE);
  
  // 初始化键盘控制
  // 重要：只有 Leonardo, Micro, Due 等板卡支持此功能[1,5](@ref)
  Keyboard.begin();
  Mouse.begin();
  // 可选：等待串口连接建立，确保通信稳定
  // while (!Serial) {
  //   delay(10);
  // }
  
  Serial.println("Arduino Keyboard Emulator Ready.");
  Serial.println("Send commands like 'up', 'down', 'f1' via Serial Monitor.");
}

void loop() {
  // 检查串口是否有数据到达[1](@ref)
  if (Serial.available() > 0) {

    int inChar = Serial.read();

        switch(status)
        {
        case findHeadFirtst:
        {
            if(inChar == 0X66)
            {
                buffer[pr] = 0X66;
                pr++;
                status = findHeadSecond;
            }
        }
        break;
        case findHeadSecond:
        {
            if(inChar == 0X68)
            {
                buffer[pr] = 0X68;
                pr++;
                status = findEndFirtst;
            }
            else
            {
                pr=0;
                status = findHeadFirtst;
            }
        }
        break;
        case findEndFirtst:
        {
            buffer[pr] = inChar;
            if(inChar == 0X5b)
            {
                status = findEndSecond;
            }
            pr++;
        }
        break;
        case findEndSecond:
        {
            buffer[pr] = inChar;
            pr++;
            if(inChar == 0X81)
            {
                uint16_t crc = crc_16(&buffer[2],pr-2-2-2);
                uint16_t recCrc;
                memcpy(&recCrc, &buffer[pr-2-2], 2);
                if (crc != recCrc)
                {
                    pr=0;
                    status = findHeadFirtst;
                }
                else
                {
                    int cmd =  buffer[2];
                    if(cmd == 1)//键盘
                    {
                        int size = buffer[3];
                        for(int i = 0;i<size;i++)
                        {
                            Keyboard.write(buffer[4+i]);
                        }
                    }
                    if(cmd == 2)
                    {
                        int type  = buffer[3];
                        if(type == 1)
                        {
                            int32_t  x = 0;
                            memcpy(&x, &buffer[4], 4);
                            int32_t  y = 0;
                            memcpy(&y, &buffer[8], 4);
                            //Mouse.move(x, y, 0);
                            Mouse.move(x, y, 0);
                        }
                        else if(type == 2)
                        {
                            Mouse.click(MOUSE_LEFT);
                        }

                        else if(type == 3)
                        {
                            Mouse.click(MOUSE_LEFT);
                            delay(150);
                            Mouse.click(MOUSE_LEFT);
                        }
                        else if(type == 4)
                        {
                            Mouse.click(MOUSE_RIGHT);
                        }
                    }
                    pr=0;
                    status = findHeadFirtst;
                }
            }
            else
            {
                status = findEndFirtst;
                pr=0;
            }
        }
        break;
        }
    // 读取来自串口的字符串，直到遇到换行符'\n'
  }
}

/**
 * @brief 发送特殊按键信号的辅助函数
 * @param key 要发送的特殊按键键值（如 KEY_UP_ARROW）
 * 
 * 该函数模拟按下并释放一个特殊按键。
 * 使用 press 和 release 的组合是为了确保按键动作的完整性[8](@ref)。
 */
void sendSpecialKey(uint8_t key) {
  Keyboard.press(key);   // 按下指定的特殊按键
  delay(50);            // 保持按下状态短暂时间，模拟真实按键
  Keyboard.release(key); // 释放该按键[8](@ref)
  // 也可以使用 Keyboard.releaseAll(); 但释放特定键更精确
}