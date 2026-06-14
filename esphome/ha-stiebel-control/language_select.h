/*
 * language_select.h — Compile-time language selection
 *
 * Selects which language file to include based on the HA_LANGUAGE_* build flag
 * set via the `language` substitution in heatingpump.yaml.
 *
 * Supported values for `language` substitution:
 *   DE  — German (default, sets -DHA_LANGUAGE_DE, falls back to lang_base.h)
 *   EN  — English (sets -DHA_LANGUAGE_EN, uses lang_en.h)
 *
 * To add a new language (e.g. French):
 *   1. Create lang_fr.h (include lang_base.h, then override LNAME_* macros)
 *   2. Add:  #elif defined(HA_LANGUAGE_FR)
 *              #include "lang_fr.h"
 *   in the chain below.
 */

#pragma once

#if defined(HA_LANGUAGE_EN)
  #include "lang_en.h"
#else
  #include "lang_base.h"
#endif
