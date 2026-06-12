/*
 * Signal Requests Dispatcher
 *
 * Selects the correct model-specific signal request table based on
 * the HA_MODEL_* preprocessor token, which is set via the
 * device_model_flag substitution in heatingpump.yaml.
 *
 * To add support for a new heat pump model:
 *   1. Create signal_requests_yourmodel.h (use signal_requests_wpl13e.h as template)
 *   2. Add an #elif branch below: #elif defined(HA_MODEL_YOURMODEL)
 *   3. Set in your heatingpump.yaml:
 *        device_model: "yourmodel"        (string — used in HA device identifier)
 *        device_model_flag: "YOURMODEL"   (token  — used in this #ifdef dispatcher)
 */

#ifndef SIGNAL_REQUESTS_MODEL_H
#define SIGNAL_REQUESTS_MODEL_H

#if defined(HA_MODEL_WPL13E)
#  include "signal_requests_wpl13e.h"

#elif defined(HA_MODEL_WPF10)
#  include "signal_requests_wpf10.h"

#else
// No model flag defined — fall back to WPL13E and emit a compile-time warning
#  pragma message("device_model_flag not set or unrecognised — falling back to WPL13E signal requests")
#  include "signal_requests_wpl13e.h"

#endif

#endif // SIGNAL_REQUESTS_MODEL_H
