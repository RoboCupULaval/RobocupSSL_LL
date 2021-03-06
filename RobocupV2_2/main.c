/*
 *  ======== main.c ========
 */

//=========================Includes============================//

//#define RELEASE   //RELEASE or DEBUG
#define HARDWARE_TEST

#include "Robocup_Define.h"
#include "Serial.h"
#include "SPI.h"
#include "pid.h"
#include "nRF24L01_driver.h"
#include "quad_driver.h"
#include "L3GD20_driver.h"
#include "LSM9DS0_driver.h"
#include "DCMotor_driver.h"
#include "arduino_driver.h"
#include "four_wheel_ctrl.h"
#include "CS_demux.h"
#include "radio_unpacker.h"
#include "testTool.h"
#include "stdio.h"



uint16_t tested = 0;
uint16_t receiveData = 0;
Bool test = false;
char lettre = ' ';
/* Flag used by idle function to check if interrupt occurred */
volatile Bool isrFlag = FALSE;


uint16_t Digital_Result =0;

//start ADC
interrupt void adc_isr(void)
{
   //discard ADCRESULT0 as part of the workaround to the 1st sample errata for rev0
   Digital_Result = ADC_readResult(HandleRobot.HandleADC, ADC_ResultNumber_0);
   ADC_clearIntFlag(HandleRobot.HandleADC, ADC_IntNumber_1);   // Clear ADCINT1 flag reinitialize for next SOC
   PIE_clearInt(HandleRobot.HandlePIE, PIE_GroupNumber_10);// Acknowledge interrupt to PIE
   return;
}

void ADC_INIT_Fn()
{
	ADC_enableBandGap(HandleRobot.HandleADC);
	ADC_enableRefBuffers(HandleRobot.HandleADC);
	ADC_powerUp(HandleRobot.HandleADC);
	ADC_enable(HandleRobot.HandleADC);
	ADC_setVoltRefSrc(HandleRobot.HandleADC, ADC_VoltageRefSrc_Int);
}

void ADC_SETUP_Fn()
{
	PIE_registerPieIntHandler(HandleRobot.HandlePIE, PIE_GroupNumber_10, PIE_SubGroupNumber_1, (intVec_t)&adc_isr);
	PIE_enableAdcInt(HandleRobot.HandlePIE, ADC_IntNumber_1); // Enable ADCINT1 in PIE
	//Note: Channel ADCINA1  will be double sampled to workaround the ADC 1st sample issue for rev0 silicon errata
	ADC_setIntPulseGenMode(HandleRobot.HandleADC, ADC_IntPulseGenMode_Prior);               //ADCINT1 trips after AdcResults latch
	ADC_enableInt(HandleRobot.HandleADC, ADC_IntNumber_1);                                  //Enabled ADCINT1
	ADC_setIntMode(HandleRobot.HandleADC, ADC_IntNumber_1, ADC_IntMode_ClearFlag);          //Disable ADCINT1 Continuous mode
	ADC_setIntSrc(HandleRobot.HandleADC, ADC_IntNumber_1, ADC_IntSrc_EOC0);                 //setup EOC0 to trigger ADCINT1 to fire
	ADC_setSocChanNumber (HandleRobot.HandleADC, ADC_SocNumber_0, ADC_SocChanNumber_A4);    //set SOC0 channel select to ADCINA4
	ADC_setSocTrigSrc(HandleRobot.HandleADC, ADC_SocNumber_0, ADC_SocTrigSrc_Sw);     //set SOC0 start trigger on EPWM1A, due to round-robin SOC0 converts first then SOC1
	ADC_setSocSampleWindow(HandleRobot.HandleADC, ADC_SocNumber_0, ADC_SocSampleWindow_7_cycles);   //set SOC0 S/H Window to 7 ADC Clock Cycles, (6 ACQPS plus 1)
}
//stop ADC

/*
 *  ======== taskFxn ========
 */
//Robot global variable
Robot_Handle HandleRobot;

//Set up function executed once before entering OS
Void SetUp(){

    volatile int status = 0;
    volatile FILE *fid;
    char buff[100];

    HandleRobot.HandleCLK = CLK_init((void *)CLK_BASE_ADDR, sizeof(CLK_Obj));
    HandleRobot.HandleGPIO = GPIO_init((void *)GPIO_BASE_ADDR, sizeof(GPIO_Obj));
    HandleRobot.HandleCpu = CPU_init((void *)NULL, sizeof(CPU_Obj));
    HandleRobot.HandleFlash = FLASH_init((void *)FLASH_BASE_ADDR, sizeof(FLASH_Obj));
    HandleRobot.HandlePll = PLL_init((void *)PLL_BASE_ADDR, sizeof(PLL_Obj));
    HandleRobot.HandleSCI = SCI_init((void *)SCIA_BASE_ADDR, sizeof(SCI_Obj));
    HandleRobot.HandleSPI = SPI_init((void *)SPIA_BASE_ADDR, sizeof(SPI_Obj));
    HandleRobot.HandleWDog = WDOG_init((void *)WDOG_BASE_ADDR, sizeof(WDOG_Obj));
    HandleRobot.HandlePwm1 = PWM_init((void *)PWM_ePWM1_BASE_ADDR, sizeof(PWM_Obj));
    HandleRobot.HandlePwm2 = PWM_init((void *)PWM_ePWM2_BASE_ADDR, sizeof(PWM_Obj));
    HandleRobot.HandlePwm3 = PWM_init((void *)PWM_ePWM3_BASE_ADDR, sizeof(PWM_Obj));
    HandleRobot.HandleADC = ADC_init((void *)ADC_BASE_ADDR, sizeof(ADC_Obj));
    HandleRobot.HandlePIE = PIE_init((void *)PIE_BASE_ADDR, sizeof(PIE_Obj));

    // ADC START
   // PIE_disable(HandleRobot.HandlePIE);
   // PIE_disableAllInts( HandleRobot.HandlePIE );

    WDOG_disable(HandleRobot.HandleWDog);

    CLK_enableAdcClock(HandleRobot.HandleCLK);
    // ADC STOP



    //Select the internal oscillator 1 as the clock source
    CLK_setOscSrc(HandleRobot.HandleCLK, CLK_OscSrc_Internal);

    // Setup the PLL for x10 /2 which will yield 50Mhz = 10Mhz * 10 / 2
    PLL_setup(HandleRobot.HandlePll, PLL_Multiplier_12, PLL_DivideSelect_ClkIn_by_2);

    CPU_disableGlobalInts(HandleRobot.HandleCpu);
    CPU_clearIntFlags(HandleRobot.HandleCpu);


    // Initialize SCIA
    scia_init(HandleRobot.HandleCLK,HandleRobot.HandleSCI);
    scia_gpio_init(HandleRobot.HandleGPIO);

    // Initialize SPI
    spi_init(HandleRobot.HandleCLK,HandleRobot.HandleSPI);
    spi_fifo_init(HandleRobot.HandleSPI);
    spi_gpio_init(HandleRobot.HandleGPIO);

    // ADC START
    ADC_INIT_Fn();
    ADC_SETUP_Fn();
        //ADC_forceConversion(myAdc, ADC_SocNumber_0);// Wait for ADC interrupt
    // ADC STOP

    demux_Init(GPIO_Number_19, GPIO_Number_12, GPIO_Number_6, CS_0);
    demux_disconnect();

    nRFInit(HandleRobot.HandleGPIO,HandleRobot.HandleSPI,GPIO_Number_7,CS_4); // 7 = CE et 6 = CSN
    HandleRobot.HandleQuad[0] = quad_init(CS_1);
    HandleRobot.HandleQuad[1] = quad_init(CS_2);

    //math float to int conversion initialisation
    robotParam_init();

    //gyro_init(CS_5);
    arduino_Init(CS_3);
    imu_init(CS_1, CS_2);

    /// PWM
    HandleRobot.HandleMotor[0] = dcMotor_init(PWM_1A, GPIO_Number_3);
    HandleRobot.HandleMotor[1] = dcMotor_init(PWM_1B, GPIO_Number_5);
    HandleRobot.HandleMotor[2] = dcMotor_init(PWM_2A, GPIO_Number_32);
    HandleRobot.HandleMotor[3] = dcMotor_init(PWM_3A, GPIO_Number_33);

    HandleRobot.HandlePid[0] = pid_init(PID_P, PID_I, PID_D, _IQ(250), _IQ(-250));
    HandleRobot.HandlePid[1] = pid_init(PID_P, PID_I, PID_D, _IQ(250), _IQ(-250));
    HandleRobot.HandlePid[2] = pid_init(PID_P, PID_I, PID_D, _IQ(250), _IQ(-250));
    HandleRobot.HandlePid[3] = pid_init(PID_P, PID_I, PID_D, _IQ(250), _IQ(-250));

    HandleRobot.HandlePid[0].term.Ref = _IQ(0);
    HandleRobot.HandlePid[1].term.Ref = _IQ(0);
    HandleRobot.HandlePid[2].term.Ref = _IQ(0);
    HandleRobot.HandlePid[3].term.Ref = _IQ(0);

    status = add_device("scia", _SSA, SCI_open, SCI_close, SCI_read, SCI_write, SCI_lseek, SCI_unlink, SCI_rename);
    fid = fopen("scia","w");
    freopen("scia:", "w", stdout);
    setvbuf(stdout, buff, _IOFBF, 100);

#ifdef HARDWARE_TEST
    test_is_SpiDevices_Working_Well();
    test_is_Wheel_System_Working_Well();
    test_is_All_Encoder_Are_Matched_With_Wheels();
    test_is_Directions_Ok();
#endif

    dcMotor_setPWM(&HandleRobot.HandleMotor[0],EPWM_BRAKE);
    dcMotor_setPWM(&HandleRobot.HandleMotor[1],EPWM_BRAKE);
    dcMotor_setPWM(&HandleRobot.HandleMotor[2],EPWM_BRAKE);
    dcMotor_setPWM(&HandleRobot.HandleMotor[3],EPWM_BRAKE);
}

/*
 *  ======== main ========
 */
Int main(){
    /*
     * use ROV->SysMin to view the characters in the circular buffer
     */
    System_printf("************enter OS************\r\n");
    BIOS_start();    /* does not return */
    return(0);
}


/*
 * ======= Test the connection of all device ======
 */
#ifdef DEBUG

_iq command = _IQ(0.3);
uint16_t pwm0 = 2000;
uint16_t pwm1 = 2000;
uint16_t pwm2 = 2000;
uint16_t pwm3 = 2000;

#endif
bool debugFlag = false;

//This function is executed every 10 ms
void Round_Robin(){
	//ADC_forceConversion(HandleRobot.HandleADC , ADC_SocNumber_0);
	//Digital_Result = ADC_readResult(HandleRobot.HandleADC, ADC_ResultNumber_0);
	System_printf("ADC: %d\r\n", Digital_Result);
	//imu_readZGyro();

#ifdef RELEASE
	bool newPacket = false;
	//***Radio Reception***
	newPacket = nRF_Listen(HandleRobot.HandleGPIO,HandleRobot.HandleSPI);

	//***Unpack***
	if(newPacket){
		debugFlag = true;
		unpackBuffer(HandleRF.RXPayload);
	}


	//***Captor read***  (Read captor first for stability) Add RobocupV2_2 which is compatible Windows and Linux. Imu driver is added, but still unstable
	quad_readCounters(&HandleRobot.HandleQuad[0]);
	quad_readCounters(&HandleRobot.HandleQuad[1]);

	/*System_printf("x=%f y=%f theta=%f\r\n",HandleRobot.robotParam.XVelocityCommand.floating
									  ,HandleRobot.robotParam.YVelocityCommand.floating
	x								  ,HandleRobot.robotParam.ThetaVelocityCommand.floating);*/
	//***Cinetic Model math***
	fourWheelCtrl_Update( _IQ(HandleRobot.robotParam.XVelocityCommand.floating),
						  _IQ(HandleRobot.robotParam.YVelocityCommand.floating),
						  _IQ(HandleRobot.robotParam.ThetaVelocityCommand.floating));


	//***PID maths***
	pid_update(&HandleRobot.HandlePid[0], _IQabs(HandleRobot.HandleQuad[0].wheelVelocity[0]));
	pid_update(&HandleRobot.HandlePid[1], _IQabs(HandleRobot.HandleQuad[0].wheelVelocity[1]));
	pid_update(&HandleRobot.HandlePid[2], _IQabs(HandleRobot.HandleQuad[1].wheelVelocity[0]));
	pid_update(&HandleRobot.HandlePid[3], _IQabs(HandleRobot.HandleQuad[1].wheelVelocity[1]));

	//***Actuator update***
	dcMotor_update(&HandleRobot.HandleMotor[0],&HandleRobot.HandlePid[0]);
	dcMotor_update(&HandleRobot.HandleMotor[1],&HandleRobot.HandlePid[1]);
	dcMotor_update(&HandleRobot.HandleMotor[2],&HandleRobot.HandlePid[2]);
	dcMotor_update(&HandleRobot.HandleMotor[3],&HandleRobot.HandlePid[3]);

#endif

#ifdef DEBUG

	quad_readCounters(&HandleRobot.HandleQuad[0]);
	quad_readCounters(&HandleRobot.HandleQuad[1]);

	HandleRobot.HandlePid[0].term.Ref = command;
	HandleRobot.HandlePid[1].term.Ref = command;
	HandleRobot.HandlePid[2].term.Ref = command;
	HandleRobot.HandlePid[3].term.Ref = command;

	pid_update(&HandleRobot.HandlePid[0], _IQabs(HandleRobot.HandleQuad[0].wheelVelocity[0]));
	pid_update(&HandleRobot.HandlePid[1], _IQabs(HandleRobot.HandleQuad[0].wheelVelocity[1]));
	pid_update(&HandleRobot.HandlePid[2], _IQabs(HandleRobot.HandleQuad[1].wheelVelocity[0]));
	pid_update(&HandleRobot.HandlePid[3], _IQabs(HandleRobot.HandleQuad[1].wheelVelocity[1]));


	dcMotor_update(&HandleRobot.HandleMotor[0],&HandleRobot.HandlePid[0]);
	dcMotor_update(&HandleRobot.HandleMotor[1],&HandleRobot.HandlePid[1]);
	dcMotor_update(&HandleRobot.HandleMotor[2],&HandleRobot.HandlePid[2]);
	dcMotor_update(&HandleRobot.HandleMotor[3],&HandleRobot.HandlePid[3]);
#endif

}

//This function is executed when roundRobin is not
void Idle(){
	if( SCI_getRxFifoStatus (HandleRobot.HandleSCI) >= SCI_FifoStatus_1_Word){
		lettre = SCI_getData (HandleRobot.HandleSCI);
		SCI_resetRxFifo(HandleRobot.HandleSCI);
	}

	if(lettre == '1'){// 500 dps
		imu_writeRegister_G(LSM9DS0_REGISTER_CTRL_REG4_G, 0x10);
	}
	if(lettre == '2'){// 2000 dps
		imu_writeRegister_G(LSM9DS0_REGISTER_CTRL_REG4_G, 0x20);
	}
	if(lettre == '3'){// 250 dps
		imu_writeRegister_G(LSM9DS0_REGISTER_CTRL_REG4_G, 0x00);
	}

	lettre = ' ';


#ifdef DEBUG
    if( SCI_getRxFifoStatus (HandleRobot.HandleSCI) >= SCI_FifoStatus_1_Word){
    		lettre = SCI_getData (HandleRobot.HandleSCI);
    		SCI_resetRxFifo(HandleRobot.HandleSCI);
    	}

    	if(lettre == '1'){
    		fourWheelCtrl_Update( _IQ(1), _IQ(0), _IQ(0));
    	}
    	if(lettre == '2'){
    		fourWheelCtrl_Update( _IQ(0), _IQ(1), _IQ(0));
    	}
    	if(lettre == '3'){
    		fourWheelCtrl_Update( _IQ(1), _IQ(1), _IQ(0));
    	}

    	lettre = ' ';

#endif
	if(debugFlag){
		System_printf("X = %f , Y = %f, Theta = %f\n\r",HandleRobot.robotParam.XVelocityCommand.floating,
				HandleRobot.robotParam.YVelocityCommand.floating,
				HandleRobot.robotParam.ThetaVelocityCommand.floating);
		debugFlag = false;
	}

	System_flush();
}


//SYS/BIOS call this function when system_flush is called
void System_Output(Char *string, UInt length){
	//send System buffer to serial.
	SCI_write(1, string,length);
}

//change values to IQ
void robotParam_init(){

	_iq buf = 0;
	_iq buf2 = 0;
	_iq op = 0;

	HandleRobot.robotParam.wheelRadius = _IQ(WHEEL_DIAMETER);
	HandleRobot.robotParam.roundRobinTime = _IQ(RRTIME);
	HandleRobot.robotParam.encoderPPR = ENCODER_PPR;
	op = _IQ(ONE_ENCODER_PPR);
	buf = _IQdiv(HandleRobot.robotParam.wheelRadius, HandleRobot.robotParam.roundRobinTime);
	buf2 = _IQmpy(buf, PI);
	HandleRobot.robotParam.speedFactor = _IQmpy(buf2, op);
	HandleRobot.robotParam.XVelocityCommand.floating = _IQ(0);
	HandleRobot.robotParam.YVelocityCommand.floating = _IQ(0);
	HandleRobot.robotParam.ThetaVelocityCommand.floating = _IQ(0);
}
