#include <Keyboard.h> // 包含键盘库，这是实现键盘模拟的核心[5](@ref)
#include <Mouse.h>
#define SERIAL_BAUD_RATE 9600

unsigned char buffer[50];
unsigned char *charptr= buffer;
int pr = 0;
int receiveDataCount = 0 ;
int receiveDataIndex = 0 ;
enum pasreDataStatus
{
    findHeadFirtst = 0,
	findHeadSecond,
    getReceiveDataCount,
    receiveData,
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

  Serial.begin(SERIAL_BAUD_RATE);
  
  Keyboard.begin();
  Mouse.begin();
  while (!Serial) {
    delay(10);
  }
    delay(1* 1000);
  Serial.println("Arduino Keyboard Emulator Ready.");
  Serial.println("Send commands like 'up', 'down', 'f1' via Serial Monitor.");
}

void loop() {
  if (Serial.available() > 0) {
    int inChar1  = Serial.read();
    uint8_t inChar = (uint8_t)inChar1;
    Serial.println("current char ");
    Serial.println(inChar,HEX);
    Serial.println("current pr ");
    Serial.println(pr);
    Serial.println("current status ");
    Serial.println(status);
        switch(status)
        {
        case findHeadFirtst:
        {
            if((unsigned int)inChar == 0X66)
            {
                buffer[pr] = 0X66;
                pr++;
                status = findHeadSecond;
            }
        }
        break;
        case findHeadSecond:
        {
            if((unsigned int)inChar == 0X68)
            {
                buffer[pr] = 0X68;
                pr++;
                status = getReceiveDataCount;
            }
            else
            {
                pr=0;
                status = findHeadFirtst;
            }
        }
        break;
        case getReceiveDataCount:
        {
            buffer[pr] = inChar;
            receiveDataCount =  inChar;
            status = receiveData;
            receiveDataIndex = 0;
            pr++;
        }
        break;
        case receiveData:
        {
            buffer[pr] = inChar;
            pr++;
            receiveDataIndex++;
            Serial.println("receiveDataIndex");
            Serial.println(receiveDataIndex);
            Serial.println("receiveDataCount");
            Serial.println(receiveDataCount);
            if(receiveDataIndex == receiveDataCount)
            {
                status = findEndFirtst;
            }
        }
        break;
        case findEndFirtst:
        {
            buffer[pr] = inChar;
            pr++;
            if(inChar == 0X5B)
            {
                Serial.println("inChar == 0X5B");
                status = findEndSecond;
            }
            else
            {
                status = findHeadFirtst;
                pr=0;
            }
        }
        break;
        case findEndSecond:
        {
            buffer[pr] = inChar;
            pr++;
            if(inChar == 0X81)
            {
                Serial.println(5);
                Serial.println(pr);
                for(int i = 0 ; i<pr ; i++)
                {
                    Serial.println(buffer[i],HEX);
                }
                uint16_t crc = crc_16(charptr+3,pr-2-2-3);
                Serial.println(crc);
                uint16_t recCrc;
                memcpy(&recCrc, &buffer[pr-2-2], 2);
                Serial.println(recCrc);
                if (crc != recCrc)
                {
                    Serial.println(6);
                    pr=0;
                    status = findHeadFirtst;
                }
                else
                {
                    int cmd =  buffer[3];
                    if(cmd == 1)//键盘
                    {
                        int type = buffer[4];
						if(type == 1)
						{
							Keyboard.press(buffer[5]);
						}
						else if(type == 2)
						{
							Keyboard.release(buffer[5]);
						}
						else if(type == 3)
						{
							int size = buffer[5];
							for(int i = 0;i<size;i++)
							{
								Keyboard.write(buffer[6+i]);
							}
						}
                    }
                    if(cmd == 2)
                    {
                        
                        int type  = buffer[4];
                        if(type == 1)
                        {
                            int32_t  x = 0;
                            memcpy(&x, &buffer[5], 4);
                            int32_t  y = 0;
                            memcpy(&y, &buffer[9], 4);
                            //Mouse.move(x, y, 0);
                             Mouse.move((int8_t)x, (int8_t)y, 0); // 修正关键点
                             Serial.println("X raw="); Serial.println(x);
                             Serial.println("Y raw="); Serial.println(y);
                             delay(2);
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
						else if(type == 5)
                        {
                            Mouse.press(MOUSE_LEFT);
                        }
						else if(type == 6)
                        {
                            Serial.println("mouse MOUSE_RIGHT press");
                            Mouse.press(MOUSE_RIGHT);
                        }
						else if(type == 7)
                        {
                            Serial.println("mouse MOUSE_LEFT release");
                            Mouse.release(MOUSE_LEFT);
                        }
						else if(type == 8)
                        {
                            Serial.println("mouse MOUSE_RIGHT release");
                            Mouse.release(MOUSE_RIGHT);
                        }
                    }
                    pr=0;
                    status = findHeadFirtst;
                }
            }
            else
            {
                status = findHeadFirtst;
                pr=0;
            }
        }
        break;
        }
  }
}
