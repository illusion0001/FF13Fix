#include "stdafx.h"
#include "XInputManager.h"

XInputManager::XInputManager(uint8_t** base_controller_input_address_ptr, const float vibrationStrengthFactor)
{
	this->vibrationStrengthFactor = vibrationStrengthFactor;
	xinputThread = std::thread(&XInputManager::Run, this, base_controller_input_address_ptr);
	spdlog::debug("base_controller_input_address_ptr = {}", (void*)base_controller_input_address_ptr);
}

void XInputManager::Run(uint8_t** base_controller_input_address_ptr)
{
	WaitAndSetVibrationAddress(base_controller_input_address_ptr);
	ScanForConnectedController();
}

void XInputManager::ScanForConnectedController()
{
	spdlog::info("Scanning for connected controllers...");

	while(true){
		for (uint32_t i = 0; i < XUSER_MAX_COUNT; i++) {
			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));

			const int32_t controllerState = XInputGetState(i, &state);
			if (controllerState == ERROR_SUCCESS) {
				spdlog::info("Connected controller {}", i);
				controllerIdShared.store(i, std::memory_order_release);
				controllerIdShared.wait(i, std::memory_order_acquire);
				spdlog::info("Controller {} disconnected. Resuming scanning...", i);
				break;
			}
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
}

void XInputManager::WaitAndSetVibrationAddress(uint8_t** base_controller_input_address_ptr)
{
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
		if (base_controller_input_address_ptr && *base_controller_input_address_ptr) {
			vibration_address_high_frequency = (float*)(*base_controller_input_address_ptr + 0x40 + 0x5C);
			vibration_address_low_frequency = (float*)(*base_controller_input_address_ptr + 0x40 + 0x60);
		}
	} while (vibration_address_low_frequency == NULL);
}

void XInputManager::SendVibrationToController()
{
	if (vibration_address_low_frequency == NULL || vibrationStrengthFactor < 0.0001f || controllerIdMainThread < 0) {
		return;
	}
	const WORD maxVibrationStrength = 65535;
	const float vibrationStrengthLowFrequency = *vibration_address_low_frequency;
	const float vibrationStrengthHighFrequency = *vibration_address_high_frequency;
	const WORD leftMotorVibration = (WORD)(std::min(vibrationStrengthFactor * vibrationStrengthLowFrequency, 1.0f) * maxVibrationStrength);
	const WORD rightMotorVibration = (WORD)(std::min(vibrationStrengthFactor * vibrationStrengthHighFrequency, 1.0f) * maxVibrationStrength);
	SetControllerVibration(leftMotorVibration, rightMotorVibration);
}

void XInputManager::SetControllerVibration(const WORD& leftMotorVibration, const WORD& rightMotorVibration)
{
	int32_t controllerToVibrate = controllerIdMainThread;

	if (controllerToVibrate >= 0) {
		LogValues();
		XINPUT_VIBRATION vibration;
		ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
		vibration.wLeftMotorSpeed = leftMotorVibration;
		vibration.wRightMotorSpeed = rightMotorVibration;
		XInputSetState(controllerIdMainThread, &vibration);
	}
}


void XInputManager::ReadValues(uint32_t* buttonBuffer, float* ff13AnalogValues) {
	if (controllerIdMainThread == -1) {
		controllerIdMainThread = controllerIdShared.load(std::memory_order_acquire);
	}
	if (controllerIdMainThread == -1) {
		return;
	}
	XINPUT_STATE xInputState;
	ZeroMemory(&xInputState, sizeof(XINPUT_STATE));
	DWORD getStateStatus = XInputGetState(controllerIdMainThread, &xInputState);
	if (getStateStatus == ERROR_SUCCESS) {
		uint16_t analogValueLXIfAboveDeadZone = ZeroAnalogIfBelowDeadZone(xInputState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		uint16_t analogValueLYIfAboveDeadZone = ZeroAnalogIfBelowDeadZone(xInputState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

		uint16_t analogValueRXIfAboveDeadZone = ZeroAnalogIfBelowDeadZone(xInputState.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		uint16_t analogValueRYIfAboveDeadZone = ZeroAnalogIfBelowDeadZone(xInputState.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

		float leftAnalogX = NormalizeXinputAnalogStick(analogValueLXIfAboveDeadZone);
		float leftAnalogY = NormalizeXinputAnalogStick(analogValueLYIfAboveDeadZone);

		float rightAnalogX = NormalizeXinputAnalogStick(analogValueRXIfAboveDeadZone);
		float rightAnalogY = NormalizeXinputAnalogStick(analogValueRYIfAboveDeadZone);

		uint32_t buttonMask = GetFF13ButtonMask(&xInputState);

		*buttonBuffer = buttonMask;
		// ff13AnalogValues contains the analog stick values. the game accesses the positions 2, 3, 6 and 7 too, but they always seem 0 and unused...
		ff13AnalogValues[0] = leftAnalogX;
		ff13AnalogValues[1] = leftAnalogY;
		
		ff13AnalogValues[4] = rightAnalogX;
		ff13AnalogValues[5] = rightAnalogY;
	}
	else if (getStateStatus == ERROR_DEVICE_NOT_CONNECTED) {
		controllerIdMainThread = -1;
		controllerIdShared.store(-1, std::memory_order_release);
		controllerIdShared.notify_all();
	}
}

uint32_t XInputManager::GetFF13ButtonMask(const XINPUT_STATE* xInputState)
{
	uint32_t ff13ButtonMask = 0;

	WORD pressedButtons = xInputState->Gamepad.wButtons;

	if (pressedButtons & XINPUT_GAMEPAD_DPAD_UP) {
		ff13ButtonMask |= FF13_GAMEPAD_DPAD_UP;
	}
	if (pressedButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
		ff13ButtonMask |= FF13_GAMEPAD_DPAD_DOWN;
	}
	if (pressedButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
		ff13ButtonMask |= FF13_GAMEPAD_DPAD_LEFT;
	}
	if (pressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
		ff13ButtonMask |= FF13_GAMEPAD_DPAD_RIGHT;
	}

	if (pressedButtons & XINPUT_GAMEPAD_Y) {
		ff13ButtonMask |= FF13_GAMEPAD_TRIANGLE;
	}
	if (pressedButtons & XINPUT_GAMEPAD_A) {
		ff13ButtonMask |= FF13_GAMEPAD_CROSS;
	}
	if (pressedButtons & XINPUT_GAMEPAD_X) {
		ff13ButtonMask |= FF13_GAMEPAD_SQUARE;
	}
	if (pressedButtons & XINPUT_GAMEPAD_B) {
		ff13ButtonMask |= FF13_GAMEPAD_CIRCLE;
	}

	if (pressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) {
		ff13ButtonMask |= FF13_GAMEPAD_L1;
	}
	if (xInputState->Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
		ff13ButtonMask |= FF13_GAMEPAD_L2;
	}
	if (pressedButtons & XINPUT_GAMEPAD_LEFT_THUMB) {
		ff13ButtonMask |= FF13_GAMEPAD_L3;
	}


	if (pressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
		ff13ButtonMask |= FF13_GAMEPAD_R1;
	}
	if (xInputState->Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
		ff13ButtonMask |= FF13_GAMEPAD_R2;
	}
	if (pressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB) {
		ff13ButtonMask |= FF13_GAMEPAD_R3;
	}

	if (pressedButtons & XINPUT_GAMEPAD_BACK) {
		ff13ButtonMask |= FF13_GAMEPAD_SELECT;
	}
	if (pressedButtons & XINPUT_GAMEPAD_START) {
		ff13ButtonMask |= FF13_GAMEPAD_START;
	}

	return ff13ButtonMask;
}

float XInputManager::NormalizeXinputAnalogStick(const int16_t value) {
	return  ((const int)value + 32768) / 32768.0f - 1.0f;
}

void XInputManager::LogValues()
{
	if (vibration_address_low_frequency == NULL) {
		return;
	}

	const float vibrationStrengthLowFrequency = *vibration_address_low_frequency;
	const float vibrationStrengthHighFrequency = *vibration_address_high_frequency;
	spdlog::info("LF({}) = {} HF({}) = {}", (void*)vibration_address_low_frequency, vibrationStrengthLowFrequency, (void*)vibration_address_high_frequency, vibrationStrengthHighFrequency);
}

int16_t XInputManager::ZeroAnalogIfBelowDeadZone(const int16_t analogValue, const int16_t deadZoneValue) {
	return abs(analogValue) > deadZoneValue ? analogValue : 0;
}