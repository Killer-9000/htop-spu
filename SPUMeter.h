#ifndef HEADER_SPUMeter
#define HEADER_SPUMeter

#define FAKE_SPU

#include "Meter.h"

typedef enum {
   SPU_METER_NICE = 0,
   SPU_METER_NORMAL = 1,
   SPU_METER_KERNEL = 2,
   SPU_METER_IRQ = 3,
   SPU_METER_SOFTIRQ = 4,
   SPU_METER_STEAL = 5,
   SPU_METER_GUEST = 6,
   SPU_METER_IOWAIT = 7,
   SPU_METER_FREQUENCY = 8,
   SPU_METER_TEMPERATURE = 9,
   SPU_METER_ITEMCOUNT = 10, // number of entries in this enum
} SPUMeterValues;

extern const MeterClass SPUMeter_class;

extern const MeterClass AllSPUsMeter_class;

extern const MeterClass AllSPUs2Meter_class;

extern const MeterClass LeftSPUsMeter_class;

extern const MeterClass RightSPUsMeter_class;

extern const MeterClass LeftSPUs2Meter_class;

extern const MeterClass RightSPUs2Meter_class;

extern const MeterClass AllSPUs4Meter_class;

extern const MeterClass LeftSPUs4Meter_class;

extern const MeterClass RightSPUs4Meter_class;

extern const MeterClass AllSPUs8Meter_class;

extern const MeterClass LeftSPUs8Meter_class;

extern const MeterClass RightSPUs8Meter_class;

#endif