/* mbed Microcontroller Library
 * Copyright (c) 2019 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PinNames.h"
#include "mbed.h"
#include "DHT.h"
#include "InputResult.h"
#include <cstdio>
#include <cstring>


// Blinking rate in milliseconds
#define SENSORS_UPDATE_RATE     2000ms
#define SEND_RATE               2000ms
#define MAIN_LOOP_RATE          200ms
#define SEND_UPDATE_RATE        300ms

const auto bufferSize = sizeof(InputResult);

// ======================================
// Ports initialization

DHT SensorAir(PB_5, DHT22); //D4 - port
AnalogIn CapacitiveSensor(PA_1); //A1 - port

BufferedSerial RsConnector(PA_11, PA_12, 9600); // D12 D13 - rigth stick (TX, RX)

// =====================================
// Led Logic

DigitalOut MainLed(LED1);

volatile bool IsMainLedBlinking = false;

void ChangeBlinkState() 
{
    if (IsMainLedBlinking) 
    {
        MainLed = 1;
        IsMainLedBlinking = false;
    } 
    else
        MainLed = 0;
}

// ======================================
// Main threads

Mail<InputResult, 4> mailbox;


void SendInfoToRS()
{
    while (true) 
    {
        auto mail = mailbox.try_get();

        if(mail)
        {
            if(!RsConnector.writable())
                continue;

            char mailBuffer[bufferSize] = {0};
            memcpy(mailBuffer, reinterpret_cast<char*>(mail), bufferSize);
            // Strange bug, if i delete this info can't decode
            auto test = reinterpret_cast<InputResult *>(mailBuffer);

            auto writeResult = RsConnector.write(mailBuffer, bufferSize);

            if(writeResult < 0)
                continue;

            IsMainLedBlinking = true;
            mailbox.free(mail);
        }

        ThisThread::sleep_for(SEND_RATE);
    }
}



void ReciveDataFromSensors()
{
    while (true) 
    {
        auto mail = mailbox.try_alloc();

        auto sensorResult = SensorAir.readData();
        auto humidity = CapacitiveSensor.read();


        if (sensorResult == 0) 
        {
            auto c = SensorAir.ReadTemperature(CELCIUS);
            auto h = SensorAir.ReadHumidity();
            mail->CodeResult = sensorResult;
            mail->Data.AirTemperature = c;
            mail->Data.AirHumidity = h;
            mail->Data.AirDewpoint = SensorAir.CalcdewPoint(c, h);
            mail->Data.AirTemperature = SensorAir.CalcdewPointFast(c, h);
            mail->Data.WaterHumidity = humidity;
        } 
        else 
            mail->CodeResult = sensorResult;
        
        mailbox.put(mail);

        ThisThread::sleep_for(SENSORS_UPDATE_RATE);
    }
}


// ======================================


Thread sendTrd;
Thread sensorTrd;

int main()
{
    sendTrd.start(callback(ReciveDataFromSensors));
    sensorTrd.start(callback(SendInfoToRS));

    while (true) 
    {
        ChangeBlinkState();
        ThisThread::sleep_for(MAIN_LOOP_RATE);
    }
}
