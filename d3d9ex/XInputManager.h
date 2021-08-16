#pragma once
#include <XInput.h>

class XInputManager
{
	const uint16_t FF13_GAMEPAD_DPAD_UP = 0x0001;
	const uint16_t FF13_GAMEPAD_DPAD_DOWN = 0x0002;
	const uint16_t FF13_GAMEPAD_DPAD_LEFT = 0x0004;
	const uint16_t FF13_GAMEPAD_DPAD_RIGHT = 0x0008;
	const uint16_t FF13_GAMEPAD_TRIANGLE = 0x0010;
	const uint16_t FF13_GAMEPAD_CROSS = 0x0020;
	const uint16_t FF13_GAMEPAD_SQUARE = 0x0040;
	const uint16_t FF13_GAMEPAD_CIRCLE = 0x0080;
	const uint16_t FF13_GAMEPAD_L1 = 0x0100;
	const uint16_t FF13_GAMEPAD_L2 = 0x0200;
	const uint16_t FF13_GAMEPAD_L3 = 0x0400;
	const uint16_t FF13_GAMEPAD_R1 = 0x0800;
	const uint16_t FF13_GAMEPAD_R2 = 0x1000;
	const uint16_t FF13_GAMEPAD_R3 = 0x2000;
	const uint16_t FF13_GAMEPAD_SELECT = 0x4000;
	const uint16_t FF13_GAMEPAD_START = 0x8000;

	float* vibration_address_high_frequency = NULL;
	float* vibration_address_low_frequency = NULL;
	
	float vibrationStrengthFactor;

	int32_t controllerIdMainThread = -1;
	std::atomic<int32_t> controllerIdShared;
	std::thread xinputThread;

public:
	XInputManager(uint8_t** base_controller_input_address_ptr, const float vibrationStrengthFactor);
	void ReadValues(uint32_t* buttonBuffer, float* ff13AnalogValues);
	void SendVibrationToController();

private:
	void Run(uint8_t** base_controller_input_address_ptr);
	int16_t ZeroAnalogIfBelowDeadZone(const int16_t analogValue, const int16_t deadZoneValue);
	float NormalizeXinputAnalogStick(int16_t value);
	uint32_t GetFF13ButtonMask(const XINPUT_STATE* xInputState);
	void ScanForConnectedController();
	void WaitAndSetVibrationAddress(uint8_t** base_controller_input_address_ptr);
	void SetControllerVibration(const WORD& leftMotorVibration, const WORD& rightMotorVibration);
	void LogValues();
};

