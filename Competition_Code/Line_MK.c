#include <project.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Motor.h"
#include "Ultra.h"
#include "Nunchuk.h"
#include "Reflectance.h"
#include "Gyro.h"
#include "Accel_magnet.h"
#include "LSM303D.h"
#include "IR.h"
#include "Beep.h"
#include "mqtt_sender.h"
#include <time.h>
#include <sys/time.h>
#include "serial1.h"
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#define ZUMO "Zumo102"

//GENERAL USE FUNCTIONS

//tank turn robot
void spin(uint8 speed, uint32 delay, bool dir)
{
	//Left
	if(dir == 1){
		MotorDirLeft_Write(1);      // set LeftMotor backward mode
		MotorDirRight_Write(0);     // set RightMotor forward mode
		PWM_WriteCompare1(speed); 
		PWM_WriteCompare2(speed);
		
	}
	
	//Right
	else{
		MotorDirLeft_Write(0);      // set LeftMotor forward mode
		MotorDirRight_Write(1);     // set RightMotor backward mode
		PWM_WriteCompare1(speed); 
		PWM_WriteCompare2(speed);
		
	}
    vTaskDelay(delay);
}

//move forward with motor control
void motor_f(uint8 speed,uint32 delay)
{
    MotorDirLeft_Write(0);      // set LeftMotor forward mode
    MotorDirRight_Write(0);     // set RightMotor forward mode
    PWM_WriteCompare1(speed+15); 
    PWM_WriteCompare2(speed); 
    vTaskDelay(delay);
}

//move back and turn
void motor_backward_left(uint8 l_speed, uint8 r_speed, uint32 delay)
{
    MotorDirLeft_Write(1);      // set LeftMotor backward mode
    MotorDirRight_Write(1);     // set RightMotor backward mode
    PWM_WriteCompare1(l_speed); 
    PWM_WriteCompare2(r_speed); 
    vTaskDelay(delay);
}

//wait for button press
void button_press()
{
    while(SW1_Read() == 1)
    {
        vTaskDelay(100);
    }
    while(SW1_Read() == 0)
    {
        vTaskDelay(100);
    }
}

//follow to line start
void line_start(struct sensors_ dig)
{
    reflectance_digital(&dig);
    while(dig.r2 + dig.r1 + dig.l1 + dig.l2 != 4)     //while all the sensors are not reading black
    {
        reflectance_digital(&dig);
        if(dig.r1 == 1)
        {
           motor_f(150,0);
        }
        else
        {
           motor_turn(0,150,1);
        }
    }
    motor_f(0, 0);  
}

//sensor calibration
long sensorCalibration(long *array,int count)
{
    long average_array[6] ={0, 0, 0, 0, 0, 0};
    int i = 0, j = 0, counter = 0;
    struct sensors_ ref;
    reflectance_read(&ref);
    while(i < count)
    {
        while(j < 20) 
        {
            reflectance_read(&ref);
            average_array[0] += ref.l3;
            average_array[1] += ref.l2;
            average_array[2] += ref.l1;
            average_array[3] += ref.r1;
            average_array[4] += ref.r2;
            average_array[5] += ref.r3;
            vTaskDelay(50);
            j++;
            counter++;
        }
        Beep(500, 100);
        button_press();
        j = 0;
        i++;
    }

    for(int k=0; k < 6; k++)
    {
        array[k] = (average_array[k]/counter) + 3000;
    }
    return(*array);
}

 //LINE FOLLOWING FUNCTIONs

void line_follow(struct sensors_ dig)
{
    int counter = 0;
    int isblack = 0;
    int miss = 0;
    reflectance_digital(&dig);
    motor_f(150, 150);
    while(counter < 2){
        reflectance_digital(&dig);
        
        if(dig.l3 + dig.l2 + dig.l1 + dig.r1 + dig.r2 + dig.r3 == 6 && isblack == 0){
            isblack = 1;
            counter++;
        }
        if(dig.l2 + dig.l3 >= 1 && dig.l1!= 1 && miss == 0)
        {
            print_mqtt(ZUMO, "/miss %d", xTaskGetTickCount());
            miss = 1;
        }
        if(dig.r2 + dig.r3 >= 1 && dig.r1!= 1 && miss == 0)
        {
            print_mqtt(ZUMO, "/miss %d", xTaskGetTickCount());
            miss = 1;
        }
        if(dig.l3 + dig.l2 + dig.l1 + dig.r1 + dig.r2 + dig.r3 < 6){
            isblack = 0;
        }
        
        if( dig.l1 + dig.r1 >= 1 && miss == 1)
        {
            print_mqtt(ZUMO, "/line %d", xTaskGetTickCount());
            miss = 0;
        }
        
        if (dig.l1 == 1 && dig.r1 == 1 && dig.l3 + dig.l2 + dig.r3 + dig.r2 == 0)  //moving forward
        {
            motor_f(225, 0);
        }
        else if(dig.l1 + dig.l2 == 2 && dig.l3 + dig.r2 + dig.r3 == 0)      //slight turn right
        {
            motor_turn(0,225,0);
        }
        else if(dig.r1 + dig.r2 == 2 && dig.l3 + dig.l2 + dig.r3 == 0)      //slight turn left
        {
            motor_turn(225,0,0);
        }
        else if(dig.l2 + dig.l3 == 2 && dig.l1 + dig.r1 + dig.r2 + dig.r3 == 0)     //greater turn right
        {
            motor_turn(0, 225, 0);          
        }
        else if(dig.r2 + dig.r3 == 2 && dig.l1 + dig.r1 + dig.l2 + dig.l3 == 0)     //greater turn left
        {
            motor_turn(225, 0, 0); 
        }
    }

}

//Line following CHALLENGE
#if 1
void zmain(void)
{
    struct sensors_ dig;    
    long threshold_dig[6] = {0, 0, 0, 0, 0, 0};
    TickType_t start;
    TickType_t end;

    BatteryLed_Write(1);
    IR_Start();
    reflectance_start();
    motor_start();

    
    vTaskDelay(100);
    reflectance_digital(&dig);
    button_press();

    sensorCalibration(threshold_dig, 2);        //calibrates the program to differentiate between black and white
    reflectance_set_threshold(threshold_dig[0], threshold_dig[1], threshold_dig[2], threshold_dig[3], threshold_dig[4], threshold_dig[5]);      //sets the threshold for defining between black or white.
    
    line_start(dig);
    print_mqtt(ZUMO, "/ready line");
    IR_wait();
    start = xTaskGetTickCount();
    print_mqtt(ZUMO, "/start %d", start);
    
    line_follow(dig);
    
    motor_stop();
    end = xTaskGetTickCount();
    print_mqtt(ZUMO, "/end %d", end);
    print_mqtt(ZUMO, "/time %d", end-start);
    
    while(1)
    {
        vTaskDelay(100); // sleep (in an infinite loop)
    }
 }   
    
#endif