/*
htop - SPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "SPUMeter.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "CRT.h"
#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "Settings.h"
#include "XUtils.h"


static const int SPUMeter_attributes[] = {
   CPU_NICE,
   CPU_NORMAL,
   CPU_SYSTEM,
   CPU_IRQ,
   CPU_SOFTIRQ,
   CPU_STEAL,
   CPU_GUEST,
   CPU_IOWAIT
};

typedef struct SPUMeterData_ {
   unsigned int spus;
   Meter** meters;
} SPUMeterData;

static void SPUMeter_init(Meter* this) {
   unsigned int spu = this->param;
   const Machine* host = this->host;
   if (spu == 0) {
      Meter_setCaption(this, "Avg");
   } else if (host->activeSPUs > 1) {
      char caption[10];
      xSnprintf(caption, sizeof(caption), "%3u", Settings_spuId(host->settings, spu - 1));
      Meter_setCaption(this, caption);
   }
}

// Custom uiName runtime logic to include the param (processor)
static void SPUMeter_getUiName(const Meter* this, char* buffer, size_t length) {
   assert(length > 0);

   if (this->param > 0)
      xSnprintf(buffer, length, "%s %u", Meter_uiName(this), this->param);
   else
      xSnprintf(buffer, length, "%s", Meter_uiName(this));
}

static void SPUMeter_updateValues(Meter* this) {
   memset(this->values, 0, sizeof(double) * SPU_METER_ITEMCOUNT);

   const Machine* host = this->host;
   const Settings* settings = host->settings;

   unsigned int spu = this->param;
   if (spu > host->existingSPUs) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "absent");
      return;
   }

   double percent = Platform_setSPUValues(this, spu);
   if (!isNonnegative(percent)) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "offline");
      return;
   }

   char spuUsageBuffer[8] = { 0 };
   char spuFrequencyBuffer[16] = { 0 };
   char spuTemperatureBuffer[16] = { 0 };

   if (settings->showSPUUsage) {
      xSnprintf(spuUsageBuffer, sizeof(spuUsageBuffer), "%.1f%%", percent);
   }

   if (settings->showSPUFrequency) {
      double spuFrequency = this->values[SPU_METER_FREQUENCY];
      if (isNonnegative(spuFrequency)) {
         xSnprintf(spuFrequencyBuffer, sizeof(spuFrequencyBuffer), "%4uMHz", (unsigned)spuFrequency);
      } else {
         xSnprintf(spuFrequencyBuffer, sizeof(spuFrequencyBuffer), "N/A");
      }
   }

   #ifdef BUILD_WITH_SPU_TEMP
   if (settings->showSPUTemperature) {
      double spuTemperature = this->values[SPU_METER_TEMPERATURE];
      if (isNaN(spuTemperature))
         xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "N/A");
      else if (settings->degreeFahrenheit)
         xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "%3d%sF", (int)(spuTemperature * 9 / 5 + 32), CRT_degreeSign);
      else
         xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "%d%sC", (int)spuTemperature, CRT_degreeSign);
   }
   #endif

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s%s%s%s",
             spuUsageBuffer,
             (spuUsageBuffer[0] && (spuFrequencyBuffer[0] || spuTemperatureBuffer[0])) ? " " : "",
             spuFrequencyBuffer,
             (spuFrequencyBuffer[0] && spuTemperatureBuffer[0]) ? " " : "",
             spuTemperatureBuffer);
}

static void SPUMeter_display(const Object* cast, RichString* out) {
   char buffer[50];
   int len;
   const Meter* this = (const Meter*)cast;
   const Machine* host = this->host;
   const Settings* settings = host->settings;

   if (this->param > host->existingSPUs) {
      RichString_appendAscii(out, CRT_colors[METER_SHADOW], " absent");
      return;
   }

   if (this->curItems == 0) {
      RichString_appendAscii(out, CRT_colors[METER_SHADOW], " offline");
      return;
   }

   len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_NORMAL]);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], ":");
   RichString_appendnAscii(out, CRT_colors[CPU_NORMAL], buffer, len);
   if (settings->detailedSPUTime) {
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sy:");
      RichString_appendnAscii(out, CRT_colors[CPU_SYSTEM], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "ni:");
      RichString_appendnAscii(out, CRT_colors[CPU_NICE_TEXT], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_IRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "hi:");
      RichString_appendnAscii(out, CRT_colors[CPU_IRQ], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_SOFTIRQ]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "si:");
      RichString_appendnAscii(out, CRT_colors[CPU_SOFTIRQ], buffer, len);
      if (isNonnegative(this->values[SPU_METER_STEAL])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_STEAL]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "st:");
         RichString_appendnAscii(out, CRT_colors[CPU_STEAL], buffer, len);
      }
      if (isNonnegative(this->values[SPU_METER_GUEST])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_GUEST]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "gu:");
         RichString_appendnAscii(out, CRT_colors[CPU_GUEST], buffer, len);
      }
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_IOWAIT]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "wa:");
      RichString_appendnAscii(out, CRT_colors[CPU_IOWAIT], buffer, len);
   } else {
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_KERNEL]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "sys:");
      RichString_appendnAscii(out, CRT_colors[CPU_SYSTEM], buffer, len);
      len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_NICE]);
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "low:");
      RichString_appendnAscii(out, CRT_colors[CPU_NICE_TEXT], buffer, len);
      if (isNonnegative(this->values[SPU_METER_IRQ])) {
         len = xSnprintf(buffer, sizeof(buffer), "%5.1f%% ", this->values[SPU_METER_IRQ]);
         RichString_appendAscii(out, CRT_colors[METER_TEXT], "vir:");
         RichString_appendnAscii(out, CRT_colors[CPU_GUEST], buffer, len);
      }
   }

   if (settings->showSPUFrequency) {
      char spuFrequencyBuffer[10];
      double spuFrequency = this->values[SPU_METER_FREQUENCY];
      if (isNonnegative(spuFrequency)) {
         len = xSnprintf(spuFrequencyBuffer, sizeof(spuFrequencyBuffer), "%4uMHz ", (unsigned)spuFrequency);
      } else {
         len = xSnprintf(spuFrequencyBuffer, sizeof(spuFrequencyBuffer), "N/A     ");
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "freq: ");
      RichString_appendnWide(out, CRT_colors[METER_VALUE], spuFrequencyBuffer, len);
   }

   #ifdef BUILD_WITH_SPU_TEMP
   if (settings->showSPUTemperature) {
      char spuTemperatureBuffer[10];
      double spuTemperature = this->values[SPU_METER_TEMPERATURE];
      if (isNaN(spuTemperature)) {
         len = xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "N/A");
      } else if (settings->degreeFahrenheit) {
         len = xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "%5.1f%sF", spuTemperature * 9 / 5 + 32, CRT_degreeSign);
      } else {
         len = xSnprintf(spuTemperatureBuffer, sizeof(spuTemperatureBuffer), "%5.1f%sC", spuTemperature, CRT_degreeSign);
      }
      RichString_appendAscii(out, CRT_colors[METER_TEXT], "temp:");
      RichString_appendnWide(out, CRT_colors[METER_VALUE], spuTemperatureBuffer, len);
   }
   #endif
}

static void AllSPUsMeter_getRange(const Meter* this, int* start, int* count) {
   unsigned int spus = this->host->existingSPUs;
   switch (Meter_name(this)[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = spus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (spus + 1) / 2;
         break;
      case 'R': // Second Half
         *start = (spus + 1) / 2;
         *count = spus - *start;
         break;
   }
}

static void AllSPUsMeter_updateValues(Meter* this) {
   SPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_updateValues(meters[i]);
}

static void SPUMeterCommonInit(Meter* this) {
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);

   SPUMeterData* data = this->meterData;
   if (!data) {
      data = this->meterData = xMalloc(sizeof(SPUMeterData));
      data->spus = this->host->existingSPUs;
      data->meters = count ? xCalloc(count, sizeof(Meter*)) : NULL;
   }

   Meter** meters = data->meters;
   for (int i = 0; i < count; i++) {
      if (!meters[i])
         meters[i] = Meter_new(this->host, start + i + 1, (const MeterClass*) Class(SPUMeter));

      Meter_init(meters[i]);
   }
}

static void SPUMeterCommonUpdateMode(Meter* this, MeterModeId mode, int ncol) {
   SPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   this->mode = mode;
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);
   if (!count) {
      this->h = 1;
      return;
   }
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   int h = meters[0]->h;
   assert(h > 0);
   this->h = h * ((count + ncol - 1) / ncol);
}

static void AllSPUsMeter_done(Meter* this) {
   SPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++)
      Meter_delete((Object*)meters[i]);
   free(data->meters);
   free(data);
}

static void SingleColSPUsMeter_updateMode(Meter* this, MeterModeId mode) {
   SPUMeterCommonUpdateMode(this, mode, 1);
}

static void DualColSPUsMeter_updateMode(Meter* this, MeterModeId mode) {
   SPUMeterCommonUpdateMode(this, mode, 2);
}

static void QuadColSPUsMeter_updateMode(Meter* this, MeterModeId mode) {
   SPUMeterCommonUpdateMode(this, mode, 4);
}

static void OctoColSPUsMeter_updateMode(Meter* this, MeterModeId mode) {
   SPUMeterCommonUpdateMode(this, mode, 8);
}

static void SPUMeterCommonDraw(Meter* this, int x, int y, int w, int ncol) {
   SPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);
   int colwidth = w / ncol;
   int diff = w % ncol;
   int nrows = (count + ncol - 1) / ncol;
   for (int i = 0; i < count; i++) {
      int d = (i / nrows) > diff ? diff : (i / nrows); // dynamic spacer
      int xpos = x + ((i / nrows) * colwidth) + d;
      int ypos = y + ((i % nrows) * meters[0]->h);
      meters[i]->draw(meters[i], xpos, ypos, colwidth);
   }
}

static void DualColSPUsMeter_draw(Meter* this, int x, int y, int w) {
   SPUMeterCommonDraw(this, x, y, w, 2);
}

static void QuadColSPUsMeter_draw(Meter* this, int x, int y, int w) {
   SPUMeterCommonDraw(this, x, y, w, 4);
}

static void OctoColSPUsMeter_draw(Meter* this, int x, int y, int w) {
   SPUMeterCommonDraw(this, x, y, w, 8);
}


static void SingleColSPUsMeter_draw(Meter* this, int x, int y, int w) {
   SPUMeterData* data = this->meterData;
   Meter** meters = data->meters;
   int start, count;
   AllSPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}


const MeterClass SPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = SPUMeter_updateValues,
   .getUiName = SPUMeter_getUiName,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = SPU_METER_ITEMCOUNT,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "SPU",
   .uiName = "SPU",
   .caption = "SPU",
   .init = SPUMeter_init
};

const MeterClass AllSPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "AllSPUs",
   .uiName = "SPUs (1/1)",
   .description = "SPUs (1/1): all SPUs",
   .caption = "SPU",
   .draw = SingleColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = SingleColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass AllSPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "AllSPUs2",
   .uiName = "SPUs (1&2/2)",
   .description = "SPUs (1&2/2): all SPUs in 2 shorter columns",
   .caption = "SPU",
   .draw = DualColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = DualColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass LeftSPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "LeftSPUs",
   .uiName = "SPUs (1/2)",
   .description = "SPUs (1/2): first half of list",
   .caption = "SPU",
   .draw = SingleColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = SingleColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass RightSPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "RightSPUs",
   .uiName = "SPUs (2/2)",
   .description = "SPUs (2/2): second half of list",
   .caption = "SPU",
   .draw = SingleColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = SingleColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass LeftSPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "LeftSPUs2",
   .uiName = "SPUs (1&2/4)",
   .description = "SPUs (1&2/4): first half in 2 shorter columns",
   .caption = "SPU",
   .draw = DualColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = DualColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass RightSPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "RightSPUs2",
   .uiName = "SPUs (3&4/4)",
   .description = "SPUs (3&4/4): second half in 2 shorter columns",
   .caption = "SPU",
   .draw = DualColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = DualColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass AllSPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "AllSPUs4",
   .uiName = "SPUs (1&2&3&4/4)",
   .description = "SPUs (1&2&3&4/4): all SPUs in 4 shorter columns",
   .caption = "SPU",
   .draw = QuadColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = QuadColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass LeftSPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "LeftSPUs4",
   .uiName = "SPUs (1-4/8)",
   .description = "SPUs (1-4/8): first half in 4 shorter columns",
   .caption = "SPU",
   .draw = QuadColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = QuadColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass RightSPUs4Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "RightSPUs4",
   .uiName = "SPUs (5-8/8)",
   .description = "SPUs (5-8/8): second half in 4 shorter columns",
   .caption = "SPU",
   .draw = QuadColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = QuadColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass AllSPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "AllSPUs8",
   .uiName = "SPUs (1-8/8)",
   .description = "SPUs (1-8/8): all SPUs in 8 shorter columns",
   .caption = "SPU",
   .draw = OctoColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = OctoColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass LeftSPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "LeftSPUs8",
   .uiName = "SPUs (1-8/16)",
   .description = "SPUs (1-8/16): first half in 8 shorter columns",
   .caption = "SPU",
   .draw = OctoColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = OctoColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};

const MeterClass RightSPUs8Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SPUMeter_display
   },
   .updateValues = AllSPUsMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .isMultiColumn = true,
   .total = 100.0,
   .attributes = SPUMeter_attributes,
   .name = "RightSPUs8",
   .uiName = "SPUs (9-16/16)",
   .description = "SPUs (9-16/16): second half in 8 shorter columns",
   .caption = "SPU",
   .draw = OctoColSPUsMeter_draw,
   .init = SPUMeterCommonInit,
   .updateMode = OctoColSPUsMeter_updateMode,
   .done = AllSPUsMeter_done
};
