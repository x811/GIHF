#include <stdafx.h>

#include <Log.h>
#include <ntstrsafe.h>

TRACELOGGING_DEFINE_PROVIDER(provider,
	"MyProvider",
	// {011DAA93-4EDD-40B9-8A41-57E4429DAEB4}
	(0x11daa93, 0x4edd, 0x40b9, 0x8a, 0x41, 0x57, 0xe4, 0x42, 0x9d, 0xae, 0xb4));

void HvLog(const char* format, ...)
{
	char message_buffer[256];

	va_list args;
	va_start(args, format);
	RtlStringCchVPrintfA(message_buffer, sizeof(message_buffer), format, args);
	va_end(args);

	TraceLoggingWrite(provider, "[*] ", TraceLoggingString(message_buffer, "Message"));
}

void Log(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vDbgPrintExWithPrefix("[GIHF] ", DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, format, args);
	va_end(args);
}
