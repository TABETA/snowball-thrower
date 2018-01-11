/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

#include "Joystick.h"

typedef enum {
	UP,
	DOWN,
	LEFT,
	RIGHT,
	X,
	Y,
	A,
	B,
	L,
	R,
	THROW,
	NOTHING,
	TRIGGERS
} Buttons_t;

typedef struct {
	Buttons_t button;
	uint16_t duration;
} command; 


typedef enum {
	SYNC_CONTROLLER,
	SYNC_POSITION,
	BREATHE,
	PROCESS,
	CLEANUP,
	DONE
} State_t;
typedef struct Report{
#define ECHOES 2
	State_t state;
	USB_JoystickReport_Input_t last_report;
	int echoes;
	int report_count;
	int xpos;
	int ypos;
	int bufindex;
	int duration_count;
	int portsval;
}Report;

void Report_init(Report* const self);
bool Report_getNext(Report* const self, USB_JoystickReport_Input_t* const ReportData, const command commands[]);

static Report report;

static const command setup[] = {
	// Setup controller
	{ NOTHING, 250 },
	{ TRIGGERS, 5 },
	{ NOTHING, 150 },
	{ TRIGGERS, 5 },
	{ NOTHING, 150 },
	{ A, 5 },
	{ NOTHING, 250 },
};

static command step[MAX_STEP] = {
#include COMMAND_FILE
};

// Main entry point.
int main(void) {
	// We'll start by performing hardware and peripheral setup.
	SetupHardware();
	// We'll then enable global interrupts for our use.
	GlobalInterruptEnable();
	// Once that's done, we'll enter an infinite loop.
	Report_init(&report);
	for (;;)
	{
		// We need to run our task to process and deliver data for our IN and OUT endpoints.
		HID_Task();
		// We also need to run the main USB management task.
		USB_USBTask();
	}
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void) {
	// We need to disable watchdog if enabled by bootloader/fuses.
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	// We need to disable clock division before initializing the USB hardware.
	clock_prescale_set(clock_div_1);
	// We can then initialize our hardware and peripherals, including the USB stack.

	#ifdef ALERT_WHEN_DONE
	// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
	#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
	DDRD  = 0xFF; //Teensy uses PORTD
	PORTD =  0x0;
                  //We'll just flash all pins on both ports since the UNO R3
	DDRB  = 0xFF; //uses PORTB. Micro can use either or, but both give us 2 LEDs
	PORTB =  0x0; //The ATmega328P on the UNO will be resetting, so unplug it?
	#endif
	// The USB stack should be initialized last.
	USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void) {
	// We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void) {
	// We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	// We setup the HID report endpoints.
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

	// We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void) {
	// We can handle two control requests: a GetReport and a SetReport.

	// Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void) {
	// If the device isn't connected and properly configured, we can't do anything here.
	if (USB_DeviceState != DEVICE_STATE_Configured)
		return;

	// We'll start with the OUT endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
	// We'll check to see if we received something on the OUT endpoint.
	if (Endpoint_IsOUTReceived())
	{
		// If we did, and the packet has data, we'll react to it.
		if (Endpoint_IsReadWriteAllowed())
		{
			// We'll create a place to store our data received from the host.
			USB_JoystickReport_Output_t JoystickOutputData;
			// We'll then take in that data, setting it up in our storage.
			while(Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError);
			// At this point, we can react to this data.

			// However, since we're not doing anything with this data, we abandon it.
		}
		// Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
		Endpoint_ClearOUT();
	}

	// We'll then move on to the IN endpoint.
	Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
	// We first check to see if the host is ready to accept data.
	if (Endpoint_IsINReady())
	{
		static bool isSetupDone = false;
		// We'll create an empty report.
		USB_JoystickReport_Input_t JoystickInputData;
		// We'll then populate this report with what we want to send to the host.
		if (isSetupDone)
		{
			Report_getNext(&report, &JoystickInputData, step);
		}
		else
		{
			Report_getNext(&report, &JoystickInputData, setup);
			isSetupDone = true;
		}
		// Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
		while(Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError);
		// We then send an IN packet on this endpoint.
		Endpoint_ClearIN();
	}
}



void Report_init(Report* const self){
	self->state = SYNC_CONTROLLER;
	self->report_count = 0;
	self->xpos = 0;
	self->ypos = 0;
	self->bufindex = 0;
	self->duration_count = 0;
	self->portsval = 0;
}

void Report_reset(Report* const self, USB_JoystickReport_Input_t* const ReportData){
	// state = CLEANUP;
	self->bufindex = 0;
	self->duration_count = 0;

	self->state = BREATHE;

	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;


	// state = DONE;
	//				state = BREATHE;
}
// Prepare the next report for the host.
bool Report_getNext(Report* const self, USB_JoystickReport_Input_t* const ReportData, const command commands[]) {
	bool retval = false;
	// Prepare an empty report
	memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
	ReportData->LX = STICK_CENTER;
	ReportData->LY = STICK_CENTER;
	ReportData->RX = STICK_CENTER;
	ReportData->RY = STICK_CENTER;
	ReportData->HAT = HAT_CENTER;

	// Repeat ECHOES times the last report
	if (self->echoes > 0)
	{
		memcpy(ReportData, &(self->last_report), sizeof(USB_JoystickReport_Input_t));
		(self->echoes)--;
		return retval;
	}

	// States and moves management
	switch (self->state)
	{

		case SYNC_CONTROLLER:
			self->state = BREATHE;
			break;

		// case SYNC_CONTROLLER:
		// 	if (report_count > 550)
		// 	{
		// 		report_count = 0;
		// 		state = SYNC_POSITION;
		// 	}
		// 	else if (report_count == 250 || report_count == 300 || report_count == 325)
		// 	{
		// 		ReportData->Button |= SWITCH_L | SWITCH_R;
		// 	}
		// 	else if (report_count == 350 || report_count == 375 || report_count == 400)
		// 	{
		// 		ReportData->Button |= SWITCH_A;
		// 	}
		// 	else
		// 	{
		// 		ReportData->Button = 0;
		// 		ReportData->LX = STICK_CENTER;
		// 		ReportData->LY = STICK_CENTER;
		// 		ReportData->RX = STICK_CENTER;
		// 		ReportData->RY = STICK_CENTER;
		// 		ReportData->HAT = HAT_CENTER;
		// 	}
		// 	report_count++;
		// 	break;

		case SYNC_POSITION:
			self->bufindex = 0;


			ReportData->Button = 0;
			ReportData->LX = STICK_CENTER;
			ReportData->LY = STICK_CENTER;
			ReportData->RX = STICK_CENTER;
			ReportData->RY = STICK_CENTER;
			ReportData->HAT = HAT_CENTER;


			self->state = BREATHE;
			break;

		case BREATHE:
			self->state = PROCESS;
			break;

		case PROCESS:
			if (commands[self->bufindex].duration == -1)
			{
				Report_reset(self, ReportData);
				retval = true;
			}
			else
			{
				switch (commands[self->bufindex].button)
				{

				case UP:
					ReportData->LY = STICK_MIN;
					break;

				case LEFT:
					ReportData->LX = STICK_MIN;
					break;

				case DOWN:
					ReportData->LY = STICK_MAX;
					break;

				case RIGHT:
					ReportData->LX = STICK_MAX;
					break;

				case A:
					ReportData->Button |= SWITCH_A;
					break;

				case B:
					ReportData->Button |= SWITCH_B;
					break;

				case R:
					ReportData->Button |= SWITCH_R;
					break;

				case THROW:
					ReportData->LY = STICK_MIN;
					ReportData->Button |= SWITCH_R;
					break;

				case TRIGGERS:
					ReportData->Button |= SWITCH_L | SWITCH_R;
					break;

				default:
					ReportData->LX = STICK_CENTER;
					ReportData->LY = STICK_CENTER;
					ReportData->RX = STICK_CENTER;
					ReportData->RY = STICK_CENTER;
					ReportData->HAT = HAT_CENTER;
					break;
				}

				(self->duration_count)++;

				if (self->duration_count > commands[self->bufindex].duration)
				{
					(self->bufindex)++;
					self->duration_count = 0;
				}

				if (self->bufindex > (int)(sizeof(commands) / sizeof(commands[0])) - 1)
				{
					Report_reset(self, ReportData);
					retval = true;
				}
			}


			break;

		case CLEANUP:
			self->state = DONE;
			break;

		case DONE:
			#ifdef ALERT_WHEN_DONE
			portsval = ~portsval;
			PORTD = portsval; //flash LED(s) and sound buzzer if attached
			PORTB = portsval;
			_delay_ms(250);
			#endif
			return retval;
	}

	// // Inking
	// if (state != SYNC_CONTROLLER && state != SYNC_POSITION)
	// 	if (pgm_read_byte(&(image_data[(xpos / 8) + (ypos * 40)])) & 1 << (xpos % 8))
	// 		ReportData->Button |= SWITCH_A;

	// Prepare to echo this report
	memcpy(&(self->last_report), ReportData, sizeof(USB_JoystickReport_Input_t));
	self->echoes = ECHOES;
	return retval;

}
