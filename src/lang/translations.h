#ifndef _INCLUDE_FKZ_TRANSLATIONS_H_
#define _INCLUDE_FKZ_TRANSLATIONS_H_

#include "utils/translations.h"

#include <string>

// SourceMod-style phrase tables for the cross-chat lines, resolved per client language.
extern mmu::Translations g_FKZTranslations;

// Translate `phrase` into the language of the client in `slot`.
std::string FKZ_Translate(int slot, const char *phrase);

#endif // _INCLUDE_FKZ_TRANSLATIONS_H_
