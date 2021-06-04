typedef struct {
    uint32_t device_count;
    IDirectInputDevice8* devices[20];
    IDirectInput8* dinput;
} Controllers;

static BOOL CALLBACK controller_input_cb(LPCDIDEVICEINSTANCE instance, LPVOID userData) {
    Controllers* controllers = (Controllers*)userData;

    IDirectInputDevice8A* device;
    IDirectInput_CreateDevice(
        &(*controllers->dinput),
        &instance->guidInstance,
        &device,
        NULL
    );
    IDirectInputDevice_SetCooperativeLevel(device, GetActiveWindow(), DISCL_NONEXCLUSIVE);
    IDirectInputDevice_SetDataFormat(device, GetdfDIJoystick());
    IDirectInputDevice_Acquire(device);

    controllers->device_count += 1;
    controllers->devices[controllers->device_count - 1] = device;

    return DIENUM_CONTINUE;
}

static Controllers controller_init(HINSTANCE instance) {
    Controllers controllers = {0};
    DirectInput8Create(
        instance,
        DIRECTINPUT_VERSION,
        &IID_IDirectInput8,
        (void**)&controllers.dinput,
        0
    );
    IDirectInput_EnumDevices(
        controllers.dinput,
        DI8DEVCLASS_GAMECTRL,
        controller_input_cb,
        (void*)&controllers,
        DIEDFL_ALLDEVICES
    );
    return controllers;
}
