#define CINTERFACE
#include <d3d9.h>

#include "spdlog/spdlog.h"
#include "MinHook.h"
#include "XInputManager.h"

namespace cinterface
{
	static XInputManager* pXInputManager;
	static uint32_t* pff13ButtonPressed;
	static float* pff13AnalogValues;

	HRESULT(STDMETHODCALLTYPE* TruePresent)(IDirect3DSwapChain9* This, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags) = nullptr;

	HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DSwapChain9* This, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
	{
		HRESULT result = TruePresent(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
		if (pXInputManager != NULL) {
			pXInputManager->ReadValues(pff13ButtonPressed, pff13AnalogValues);
			pXInputManager->SendVibrationToController();
		}
		return result;
	}

	void InitHookPresent(IDirect3DSwapChain9* pDirect3DSwapChain9)
	{
		if (pDirect3DSwapChain9->lpVtbl->Present && TruePresent == nullptr)
		{
			spdlog::info("Enabling PresentHook!");
			const MH_STATUS createHookPresent = MH_CreateHook(pDirect3DSwapChain9->lpVtbl->Present, HookPresent, reinterpret_cast<void**>(&TruePresent));
			spdlog::info("CreateHookPresent= {}", createHookPresent);
			const MH_STATUS enableHookPresent = MH_EnableHook(pDirect3DSwapChain9->lpVtbl->Present);
			spdlog::info("EnableHookPresent = {}", enableHookPresent);
	
		}
	}

	void ShareValuesWithSwapChainHook(XInputManager* xInputManager, uint32_t* ff13ButtonPressed, float* ff13AnalogValues) {
		pXInputManager = xInputManager;
		pff13ButtonPressed = ff13ButtonPressed;
		pff13AnalogValues = ff13AnalogValues;
	}
}


