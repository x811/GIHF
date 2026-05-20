#include <stdafx.h>

typedef struct _INTERRUPT_COMMAND_REGISTER_EXTENDED
{
	union
	{
		struct
		{
			uint64_t Vector : 8;
			uint64_t DeliveryMode : 3;
			uint64_t DestinationMode : 1;
			uint64_t reserved0 : 2;
			uint64_t Level : 1;
			uint64_t TriggerMode : 1;
			uint64_t reserved1 : 2;
			uint64_t DestinationShorthand : 2;
			uint64_t reserved2 : 12;
			uint64_t DestinationField : 32;
		};

		uint64_t Flags;
	};
} INTERRUPT_COMMAND_REGISTER_EXTENDED, *PINTERRUPT_COMMAND_REGISTER_EXTENDED;

/*
	For some reason, the only valid mode to deliver NMI is to make
	an IPI with the Delivery Mode NMI, Trigger Mode Level, and the
	Shorthand of Destination should be all excluding self.

	I am not entirely sure if this is true, but Intel states it that
	way...
*/

void xApicConfig::TriggerNmi()
{
	INTERRUPT_COMMAND_REGISTER_EXTENDED ICR{ 0 };

	ICR.DeliveryMode = 4; // Quick constant for NMI Delivery Mode
	ICR.TriggerMode = 1; //Level Trigger Mode
	ICR.DestinationMode = 0;
	ICR.Level = 1;
	ICR.DestinationShorthand = 3; // All excluding Self
	
	//Quick constant for ICR MSR in x2APIC mode
	__writemsr(0x830, ICR.Flags);
}
