#include "pch.h"
#include <memory>
#include <stdio.h>
#include <Windows.h>
#include <winevt.h>

#pragma comment(lib, "wevtapi.lib")

int ConvertToUtf8(const wchar_t* wide_text, char* utf8_text, 
    int output_buffer_size) {
    return WideCharToMultiByte(CP_UTF8, 0, wide_text, -1, utf8_text, 
        output_buffer_size, NULL, NULL);
}

DWORD GetChannelNamesInternal(char* output_buffer, int output_buffer_size, 
    char* error_message_buffer, int error_message_buffer_size);
DWORD IsChannelDisabledInternal(const wchar_t* channel_name);

extern "C" {
    __declspec(dllexport) DWORD GetChannelNames(unsigned char* output_buffer, 
        int output_buffer_size, unsigned char* error_message_buffer, 
        int error_message_buffer_size) {
        return GetChannelNamesInternal(
            (char*)output_buffer, 
            output_buffer_size, 
            (char*)error_message_buffer, 
            error_message_buffer_size);
    }
    __declspec(dllexport) DWORD IsChannelDisabled(const wchar_t* channel_name) {
        return IsChannelDisabledInternal(channel_name);
    }
}

int setErrorMessage(char* error_message_buffer, int error_message_buffer_size, 
    const wchar_t* format, ...) {
    wchar_t* temp_buffer = (wchar_t*) new wchar_t[error_message_buffer_size];
    va_list args;
    va_start(args, format);
    int result = _vsnwprintf_s(
        (wchar_t*)temp_buffer,
        (size_t)error_message_buffer_size,
        _TRUNCATE,
        format,
        args
    );
    va_end(args);
    if (result == 0)
        return 0;
    result = ConvertToUtf8(temp_buffer, error_message_buffer, error_message_buffer_size);
    delete temp_buffer;
    return result;
}

DWORD GetChannelNamesInternal(char* output_buffer, int output_buffer_size, 
    char* error_message_buffer, int error_message_buffer_size)
{
    static const int UTF8_BUFFER_SIZE = 1000;
    EVT_HANDLE hChannels = NULL;
    LPWSTR pBuffer = NULL;
    LPWSTR pTemp = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;
    DWORD status = ERROR_SUCCESS;

    error_message_buffer[0] = 0;

    int output_buffer_idx = 0;
    // Get a handle to an enumerator that contains all the names of the 
    // channels registered on the computer.
    hChannels = EvtOpenChannelEnum(NULL, 0);

    if (NULL == hChannels)
    {
        status = GetLastError();
        setErrorMessage(error_message_buffer, error_message_buffer_size, 
            L"(%d) EvtOpenChannelEnum failed", status);
        goto cleanup;
    }

    // Enumerate through the list of channel names. If the buffer is not big
    // enough reallocate the buffer. To get the configuration information for
    // a channel, call the EvtOpenChannelConfig function.
    while (true)
    {
        if (!EvtNextChannelPath(hChannels, dwBufferSize, pBuffer, &dwBufferUsed))
        {
            status = GetLastError();

            if (ERROR_NO_MORE_ITEMS == status)
            {
                break;
            }
            else if (ERROR_INSUFFICIENT_BUFFER == status)
            {
                dwBufferSize = dwBufferUsed;
                pTemp = (LPWSTR)realloc(pBuffer, dwBufferSize * sizeof(WCHAR));
                if (pTemp)
                {
                    pBuffer = pTemp;
                    pTemp = NULL;
                    EvtNextChannelPath(hChannels, dwBufferSize, pBuffer, &dwBufferUsed);
                }
                else
                {
                    status = ERROR_OUTOFMEMORY;
                    setErrorMessage(error_message_buffer, error_message_buffer_size, 
                        L"(%d) realloc failed", status);
                    goto cleanup;
                }
            }
            else
            {
                setErrorMessage(error_message_buffer, error_message_buffer_size, 
                    L"(%d) EvtNextChannelPath failed", status);
                break;
            }
        }

        if (output_buffer_idx + dwBufferUsed > output_buffer_size - 1) {
            setErrorMessage(error_message_buffer, error_message_buffer_size, 
                L"(%d) Buffer too small", ERROR_INSUFFICIENT_BUFFER);
            break;
        }
        else {
            int num_chars = ConvertToUtf8(pBuffer, output_buffer + output_buffer_idx, 
                output_buffer_size - output_buffer_idx);
            output_buffer_idx += num_chars;
        }
    }

cleanup:
    if (hChannels)
        EvtClose(hChannels);

    if (pBuffer)
        free(pBuffer);

    return output_buffer_idx;
}

DWORD IsChannelDisabledInternal(const wchar_t* channel_name) {
    // returns 0 if channel enabled, ~0 if disabled, otherwise error code
    EVT_HANDLE hChannel = NULL;
    DWORD status = ERROR_SUCCESS;
    PEVT_VARIANT pProperty = NULL;  // Buffer that receives the property value
    PEVT_VARIANT pTemp = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwBufferUsed = 0;


    hChannel = EvtOpenChannelConfig(NULL, channel_name, 0);

    if (NULL == hChannel) // Fails with 15007 (ERROR_EVT_CHANNEL_NOT_FOUND) 
                          // if the channel is not found
    {
        auto status = GetLastError();
        if (status == ERROR_SUCCESS) {
            // this didn't work, can't return that
            return ERROR_INVALID_TARGET_HANDLE;
        }
        return status;
        goto cleanup;
    }

    if (!EvtGetChannelConfigProperty(hChannel, EvtChannelConfigEnabled, 0, 
        dwBufferSize, pProperty, &dwBufferUsed))
    {
        status = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == status)
        {
            dwBufferSize = dwBufferUsed;
            pTemp = (PEVT_VARIANT)realloc(pProperty, dwBufferSize);
            if (pTemp)
            {
                pProperty = pTemp;
                pTemp = NULL;
                if (!EvtGetChannelConfigProperty(hChannel, EvtChannelConfigEnabled, 
                    0, dwBufferSize, pProperty, &dwBufferUsed)) {
                    status = GetLastError();
                }
                else {
                    status = ERROR_SUCCESS;
                }
            }
            else
            {
                status = ERROR_OUTOFMEMORY;
                goto cleanup;
            }
        }
    }
    if (ERROR_SUCCESS == status)
    {
        if (pProperty != NULL && pProperty->BooleanVal) {
            status = 0;
        }
        else {
            status = ~0;
        }
    }

cleanup:

    if (hChannel)
        EvtClose(hChannel);

    return status;
}
